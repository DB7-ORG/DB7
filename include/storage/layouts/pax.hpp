#pragma once

#include "common.hpp"
#include "shared/types/type_defs.hpp"
#include "storage/data_chunk.hpp"
#include "storage/storage_common.hpp"

/**
  PAX PAGE LAYOUT

  A page holds up to `capacity` tuples. Every live column gets a minipage sized
  for exactly `capacity` values, so the address of any value is pure arithmetic:

      value(col, i) = page + ColumnOffset(col) + i * SizeOf(type)

  Variable-length values that don't fit in their slot spill into a heap at the
  end of the page.

  +---------------------------------------------------------------+ 0
  | Header                                                        |
  |   column count         sizeof(column_t)                       |
  |   tuple count          u16   tuples currently stored          |
  |   capacity             u16   max tuples, fixed at Init        |
  |   current heap offset  u16   start of the used heap           |
  |   max heap offset      u16   end of the minipages; the heap   |
  |                              must not grow below this         |
  |   column offsets       u16 * column count (includes dropped)  |
  |     == 0          -> column dropped, it has no minipage       |
  |     bit 15 set    -> column is nullable (has a null bitmap)   |
  |     bits 0..14    -> byte offset of the column's minipage     |
  +---------------------------------------------------------------+
  | Minipages, one per live column, in column order               |
  |   padding         align start to SizeOf(type)                 |
  |   values          capacity * SizeOf(type) bytes               |
  |   null bitmap     ceil(capacity / 8) bytes, nullable only     |
  |                   bit i == 1 -> tuple i is NULL               |
  |                   zeroed at Init (all values not NULL)        |
  +---------------------------------------------------------------+ max heap offset
  | free space                                                    |
  +---------------------------------------------------------------+ current heap offset
  | heap: spilled varlen bytes, grows toward lower addresses      |
  +---------------------------------------------------------------+ DB7_PAGE_SIZE

  Varlen slot (SizeOf(VARCHAR) bytes, value includes its length prefix):
    value fits in the slot  -> [value bytes .................]
    value is larger         -> [(first slot-2) bytes][u16 heap offset]
                               the remaining bytes live at the heap offset

  NULLs: the value slot of a NULL is left unspecified, only its bitmap bit is set.
  A column without a bitmap (bit 15 clear) contains no NULLs on this page. Pages
  written before a column became nullable have no bitmap for it, so inserting a
  NULL into such a page is rejected (see Insert).
 */

namespace db7::storage {
class PaxHeader {
public:
  /** ColumnCount + TupleCount + Capacity + CurrHeapOffset + MaxHeapOffsetRef */
  static constexpr u16 fixed_header_size_ =
      sizeof(column_t) + sizeof(u16) + sizeof(u16) + sizeof(u16) + sizeof(u16);

  static constexpr u16 offset_nullable_mask_ = 1 << (sizeof(u16) * 8 - 1);

  static_assert(DB7_PAGE_SIZE <= offset_nullable_mask_,
                "Page size must be below 32KB in order for this layout to work");

private:
  static column_t &ColumnCountRef(byte *page) { return *reinterpret_cast<column_t *>(page); }

  static u16 &TupleCountRef(byte *page) {
    return *reinterpret_cast<u16 *>(page + sizeof(column_t));
  }

  static u16 &CapacityRef(byte *page) {
    return *reinterpret_cast<u16 *>(page + sizeof(column_t) + sizeof(u16));
  }

  static u16 &CurrHeapOffsetRef(byte *page) {
    return *reinterpret_cast<u16 *>(page + sizeof(column_t) + sizeof(u16) + sizeof(u16));
  }

  static u16 &MaxHeapOffsetRef(byte *page) {
    return *reinterpret_cast<u16 *>(page + sizeof(column_t) + sizeof(u16) + sizeof(u16) +
                                    sizeof(u16));
  }

public:
  static column_t ColumnCount(byte *page) { return ColumnCountRef(page); }

  static void SetColumnCount(byte *page, column_t count) { ColumnCountRef(page) = count; }

  static u16 TupleCount(byte *page) { return TupleCountRef(page); }

  static void SetTupleCount(byte *page, u16 count) { TupleCountRef(page) = count; }

  static u16 Capacity(byte *page) { return CapacityRef(page); }

  static void SetCapacity(byte *page, u16 capacity) { CapacityRef(page) = capacity; }

  static u16 CurrHeapOffset(byte *page) { return CurrHeapOffsetRef(page); }

  static void SetCurrHeapOffset(byte *page, u16 capacity) { CurrHeapOffsetRef(page) = capacity; }

  static u16 MaxHeapOffset(byte *page) { return MaxHeapOffsetRef(page); }

  static void SetMaxHeapOffset(byte *page, u16 capacity) { MaxHeapOffsetRef(page) = capacity; }

  static u16 *Offsets(byte *page) {
    byte *data = page + PaxHeader::fixed_header_size_;
    return reinterpret_cast<u16 *>(data);
  }
};

// TODO storage test with adding/dropping a column
// TODO need nullbitmap per column
// TODO storage should try to make one dto abstraction for column data so i dont have 20 different
// mappings

struct ColumnStatistics { // TODO should be moved to layers above likely catalog
  type_id type;
  bool isDropped;
  // WARNING: avg_size should take into account bytes used for setting the len of the string
  u16 avg_size;

  bool nullable;

  ColumnStatistics(type_id type, bool isDropped, u16 avg_size, bool nullable)
      : type(type), isDropped(isDropped), avg_size(avg_size), nullable(nullable) {}
};

struct PaxInsertInfo {
  u16 size;
  u16 slot_size;
  type_id type;
};

class Pax {
private:
  static constexpr u8 varlen_size = SizeOf(type_id::VARCHAR);
  static constexpr u16 invalid_offset = 0;
  static constexpr u16 inline_ptr_len = Pax::varlen_size - sizeof(u16);

  /**
   * Largest N such that the whole layout fits in a page. Computed in bits because
   * each tuple costs a fraction of a byte (one bit per nullable column).
   */
  static size_t CalculateNumTuples(size_t row_heap_size, size_t row_size, size_t padding,
                                   size_t col_count, size_t nullable_count) {
    const size_t fixed = PaxHeader::fixed_header_size_ + col_count * sizeof(u16) + padding;
    DB7_ASSERT(fixed < DB7_PAGE_SIZE, "page header and padding do not fit in the page");

    const size_t avail_bits = (DB7_PAGE_SIZE - fixed) * 8;
    const size_t rounding_bits = nullable_count * 7;
    DB7_ASSERT(avail_bits > rounding_bits, "no space left for tuples");

    const size_t bits_per_row = (row_heap_size + row_size) * 8 + nullable_count;
    const size_t N = (avail_bits - rounding_bits) / bits_per_row;
    DB7_ASSERT(N >= 1, "number of tuples in the page must be atleast 1");
    DB7_ASSERT(N <= std::numeric_limits<u16>::max(), "invalid page capacity");
    return N;

    // const auto offsets = col_count * sizeof(u16);
    // const auto bitmap1 = nullable_count * 7 / 8;
    // const auto bitmap2 = nullable_count / 8;

    // auto N = (DB7_PAGE_SIZE - PaxHeader::fixed_header_size_ - alignment - offsets - bitmap1) /
    //          (row_heap_size + row_size + bitmap2);
    // DB7_ASSERT(N >= 1, "number of tuples in the page must be atleast 1");
    // DB7_ASSERT(N <= std::numeric_limits<u16>::max(), "invalid page capacity");
    // return N;
  }

  /**
   * Append varlen data only to the heap area at the bottom of the page
   * NOTE: Doesnt check if it has space should be done in component above
   *
   * @param ptr bytes and size of data that needs to be on heap for string "123456..."
   *            ptr represent the part that is not inlined in the slot. In case of slot
   *            size == 16 then we would have a str ".16.17.18..."
   *            (dots are for splitting the string)
   */
  static u16 AppendHeapUnsafe(byte *page, std::span<byte> ptr) {
    u16 curr_heap_offset = PaxHeader::CurrHeapOffset(page);
    const u16 new_heap = curr_heap_offset - ptr.size_bytes();
    std::memcpy(page + new_heap, ptr.data(), ptr.size_bytes());
    PaxHeader::SetCurrHeapOffset(page, new_heap);
    return new_heap;
  }

  /**
   * Append varlen data to the heap area and also to the slot (inlined part)
   * NOTE: Doesnt check if it has space should be done in component above
   *
   * @param dest    slot destination in the page
   * @param payload pointer to the varlen data usually from the DataChunk
   */
  static void AppendVarlenUnsafe(byte *page, byte *dest, std::span<byte> payload) {
    /* Appends first part of the slot [len] [inline data] [heap offset(empty)] */
    std::memcpy(dest, payload.data(), inline_ptr_len);

    /* Appends to the heap area */
    u16 heap_ptr_len = payload.size() - inline_ptr_len;
    byte *heap_ptr = payload.data() + inline_ptr_len;
    u16 off = AppendHeapUnsafe(page, {heap_ptr, heap_ptr_len});

    /* Appends the offset [len] [inline data] [heap offset] */
    std::memcpy(dest + inline_ptr_len, &off, sizeof(u16));
  }

  static u16 CalcHeapBytes(u16 size) {
    return size > Pax::varlen_size ? size - (Pax::varlen_size - sizeof(u16)) : 0;
  }

  static u16 CalcHeapBytes(type_id type, u16 size) {
    return (IsVarlen(type) && size > Pax::varlen_size) ? size - (Pax::varlen_size - sizeof(u16))
                                                       : 0;
  }

  static byte *ColBitmap(byte *page, u16 offset, u8 type_size, u16 capacity) {
    return page + offset + capacity * type_size;
  }

  /* {0 -> not null , 1 -> null} */
  static bool IsNull(byte *bitmap, u16 idx) {
    byte mask = 1 << (idx % 8);
    return bitmap[idx / 8] & mask;
  }

  template <bool Null>
  static void SetBit(byte *bitmap, u16 idx) {
    byte mask = 1 << (idx % 8);
    u16 pos = idx / 8;
    if constexpr (Null) {
      bitmap[pos] |= mask;
    } else {
      bitmap[pos] &= ~mask;
    }
  }

  static u16 StripOff(u16 offset) {
    return static_cast<u16>(offset & ~PaxHeader::offset_nullable_mask_);
  }

public:
  /**
   * Method used for calculating a required heap size for some DataChunk
   *
   * @param columns column metadata
   */
  static u16 CalcHeapBytes(std::vector<PaxInsertInfo> &columns) {
    u16 total = 0;
    for (const auto &col : columns) { total += CalcHeapBytes(col.type, col.size); }
    return total;
  }

  /**
   * Method used for initializing an empty page when its first available
   *
   * NOTE: make sure pages are zeroed or do it manually here (TODO storage)
   * (we dont need to do this since we zero bitmap on insert)
   *
   * @param page   page bytes
   * @param stats  column metadata including avg_columns size which is useful for calculating
   *               a fixed layout
   */
  static void Init(byte *page, std::vector<ColumnStatistics> &stats) {
    DB7_ASSERT(stats.size() >= 1, "number of columns in the page must be atleast 1");
    PaxHeader::SetColumnCount(page, stats.size());
    PaxHeader::SetTupleCount(page, 0);
    PaxHeader::SetCurrHeapOffset(page, DB7_PAGE_SIZE);

    /** Calculates cost of each row(size) for regular minipage and heap */
    size_t row_heap_size = 0;
    size_t row_size = 0;
    size_t valid_column_count = 0;
    size_t nullable_count = 0;
    for (const auto &s : stats) {
      if (s.isDropped) continue;
      if (IsVarlen(s.type) && s.avg_size > Pax::varlen_size) {
        row_heap_size += CalcHeapBytes(s.avg_size);
      }
      row_size += SizeOf(s.type);
      valid_column_count++;
      nullable_count += size_t(s.nullable);
    }
    DB7_ASSERT(row_size > 0, "table has no live columns");
    DB7_ASSERT(valid_column_count >= 1, "number of valid columns in the page must be atleast 1");

    /**
     * Worst case padding for INT is 4-1 bytes.
     * With that logic worst case padding for an entire
     * row is row_size - number of columns
     */
    auto worst_case_padding = row_size - valid_column_count;
    size_t N = CalculateNumTuples(row_heap_size, row_size, worst_case_padding, stats.size(),
                                  nullable_count);
    PaxHeader::SetCapacity(page, N);

    u16 *offsets = PaxHeader::Offsets(page);
    size_t start = stats.size() * sizeof(u16) + PaxHeader::fixed_header_size_;
    int i = 0;
    for (const auto &s : stats) {
      if (s.isDropped) {
        offsets[i] = Pax::invalid_offset;
      } else {
        const auto col_size = SizeOf(s.type);
        start = shared::AlignUp(start, col_size);
        u16 offset_nullable = s.nullable ? PaxHeader::offset_nullable_mask_ : 0;
        offsets[i] = static_cast<u16>(start) | offset_nullable;
        const auto null_bitmap_size = s.nullable ? (N + 7) / 8 : 0;
        start += N * col_size + null_bitmap_size;
      }
      i++;
    }
    DB7_ASSERT(start + N * row_heap_size <= DB7_PAGE_SIZE, "layout overflows page");
    PaxHeader::SetMaxHeapOffset(page, start);
  }

  /**
   * Method for inserting data chunk into a page that uses pax layout.
   * Data chunk should contain all values except columns that are dropped.
   * Columns with default value should be provided.
   *
   * @param page          page bytes
   * @param chunk         data we want to insert to the page
   * @param columns       metadata for columns like size and type of the column
   * @param required_heap heap required in bytes for data to fit into this page
   */
  static bool Insert(byte *page, DataChunk *chunk, std::vector<PaxInsertInfo> &columns,
                     size_t required_heap) {
    if (required_heap > PaxHeader::CurrHeapOffset(page) - PaxHeader::MaxHeapOffset(page))
      return false;

    /** Is there still space in the page */
    const u16 ntup = PaxHeader::TupleCount(page);
    const u16 capacity = PaxHeader::Capacity(page);
    if (ntup >= capacity) { return false; }

    const column_t ncols = PaxHeader::ColumnCount(page);
    // DB7_ASSERT(ncols <= chunk->GetColCount(),
    //            "U tried to insert to an old page that doesnt have all your columns");

    /** Get offset array from page and column_ids array from chunk */
    u16 *offsets = PaxHeader::Offsets(page);
    column_t *column_ids = chunk->ColumnIds();

    /** Scan over every column in chunk */
    for (size_t i = 0; i < chunk->GetColCount(); ++i) {
      column_t col_id = column_ids[i];
      DB7_ASSERT(col_id < ncols, "Column tried to write outside of range");

      /** // TODO storage
       * If offset is invalid that means this column was deleted ignore it
       * should probably consider aborting since the catalog changed the table schema
       * and we still have the stale data
       */
      u16 offset = offsets[col_id];
      if (offset == Pax::invalid_offset) continue;

      const auto column = columns[i];
      /** Get data from the chunk that will be copied to the page */
      byte *ptr = chunk->GetByIdxNullCheck(i);
      u16 stripped_offset = StripOff(offset);
      byte *bitmap = ColBitmap(page, stripped_offset, column.slot_size, capacity);
      // TODO here fix
      if (ptr == nullptr) {
        /* if bitmap doesnt exist this is likely an old page. Treat it as if there is no space in
         * the page and make sure to mark it in the FSM as zero space hint */
        if (!(offset & PaxHeader::offset_nullable_mask_)) return false;
        SetBit<true>(bitmap, ntup);
        continue;
      } else {
        SetBit<false>(bitmap, ntup);
      }

      byte *dest = page + StripOff(offsets[column_ids[i]]) + ntup * column.slot_size;
      if (!IsVarlen(column.type) || column.size <= Pax::varlen_size) { // should be based on type
        DB7_ASSERT(IsVarlen(column.type) || column.size == column.slot_size,
                   "fixed-width value size mismatch");
        std::memcpy(dest, ptr, column.size);
      } else {
        AppendVarlenUnsafe(page, dest, {ptr, column.size});
      }
    }

    PaxHeader::SetTupleCount(page, ntup + 1);

    return true;
  }
};
} // namespace db7::storage