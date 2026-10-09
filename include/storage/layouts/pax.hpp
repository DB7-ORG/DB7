#pragma once

#include "common.hpp"
#include "shared/types/type_defs.hpp"
#include "storage/data_chunk.hpp"
#include "storage/storage_common.hpp"

namespace db7::storage {
class PaxHeader {
public:
  /** ColumnCount + TupleCount + Capacity + CurrHeapOffset + MaxHeapOffsetRef */
  static constexpr u16 fixed_header_size_ =
      sizeof(column_t) + sizeof(u16) + sizeof(u16) + sizeof(u16) + sizeof(u16);

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

  ColumnStatistics(type_id type, bool isDropped, u16 avg_size)
      : type(type), isDropped(isDropped), avg_size(avg_size) {}
};

struct PaxInsertInfo {
  u16 size;
  type_id type;
};

class Pax {
private:
  static constexpr u8 varlen_size = SizeOf(type_id::VARCHAR);
  static constexpr u16 invalid_offset = 0;
  static constexpr u16 inline_ptr_len = Pax::varlen_size - sizeof(u16);

  static size_t CalculateNumTuples(size_t row_heap_size, size_t row_size, size_t alignment,
                                   size_t col_count) {
    auto N = (DB7_PAGE_SIZE - PaxHeader::fixed_header_size_ - alignment - col_count * sizeof(u16)) /
             (row_heap_size + row_size);
    DB7_ASSERT(N >= 1, "number of tuples in the page must be atleast 1");
    DB7_ASSERT(N <= std::numeric_limits<u16>::max(), "invalid page capacity");
    return N;
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
    for (const auto &s : stats) {
      if (s.isDropped) continue;
      if (IsVarlen(s.type) && s.avg_size > Pax::varlen_size) {
        row_heap_size += CalcHeapBytes(s.avg_size);
      }
      row_size += SizeOf(s.type);
      valid_column_count++;
    }
    DB7_ASSERT(row_size > 0, "table has no live columns");
    DB7_ASSERT(valid_column_count >= 1, "number of valid columns in the page must be atleast 1");

    /**
     * Worst case padding for INT is 4-1 bytes.
     * With that logic worst case padding for an entire
     * row is row_size - number of columns
     */
    auto worst_case_padding = row_size - valid_column_count;
    size_t N = CalculateNumTuples(row_heap_size, row_size, worst_case_padding, stats.size());
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
        offsets[i] = static_cast<u16>(start);
        start += N * col_size;
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
    if (ntup >= PaxHeader::Capacity(page)) { return false; }

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

      /** Get data from the chunk that will be copied to the page */
      byte *ptr = chunk->GetByIdxNullCheck(i);
      DB7_ASSERT(ptr != nullptr, "NULLs need the per-column null bitmap (TODO storage)");
      const auto column = columns[i];

      byte *dest = page + offsets[column_ids[i]] + ntup * SizeOf(column.type);
      if (!IsVarlen(column.type) || column.size <= Pax::varlen_size) { // should be based on type
        DB7_ASSERT(IsVarlen(column.type) || column.size == SizeOf(column.type),
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