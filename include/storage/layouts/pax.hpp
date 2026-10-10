#pragma once

#include "common.hpp"
#include "shared/types/type_defs.hpp"
#include "storage/data_chunk.hpp"
#include "storage/storage_common.hpp"

// TODO storage should add response enum so i know whats the problem in layers above

// TODO storage should try to make one dto abstraction for column data so i dont have 20 different
// mappings

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
                                   size_t col_count, size_t nullable_count);

  /**
   * Append varlen data only to the heap area at the bottom of the page
   * NOTE: Doesnt check if it has space should be done in component above
   *
   * @param ptr bytes and size of data that needs to be on heap for string "123456..."
   *            ptr represent the part that is not inlined in the slot. In case of slot
   *            size == 16 then we would have a str ".16.17.18..."
   *            (dots are for splitting the string)
   */
  static u16 AppendHeapUnsafe(byte *page, std::span<byte> ptr);

  /**
   * Append varlen data to the heap area and also to the slot (inlined part)
   * NOTE: Doesnt check if it has space should be done in component above
   *
   * @param dest    slot destination in the page
   * @param payload pointer to the varlen data usually from the DataChunk
   */
  static void AppendVarlenUnsafe(byte *page, byte *dest, std::span<byte> payload);

  static u16 CalcHeapBytes(u16 size);

  static u16 CalcHeapBytes(type_id type, u16 size);

  static byte *ColBitmap(byte *page, u16 offset, u8 type_size, u16 capacity);

  /* {0 -> not null , 1 -> null} */
  static bool IsNull(byte *bitmap, u16 idx);

  template <bool Null>
  static void SetBit(byte *bitmap, u16 idx);

  static u16 StripOff(u16 offset);

public:
  /**
   * Method used for calculating a required heap size for some DataChunk
   *
   * @param columns column metadata
   */
  static u16 CalcHeapBytes(std::vector<PaxInsertInfo> &columns);

  /**
   * Method used for initializing an empty page when its first available
   *
   * @param page   page bytes
   * @param stats  column metadata including avg_columns size which is useful for calculating
   *               a fixed layout
   */
  static void Init(byte *page, std::vector<ColumnStatistics> &stats);

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
                     size_t required_heap, u16 catalog_col_count);

  // static void Get(byte* page, DataChunk *chunk, std::vector<PaxInsertInfo> &columns)
};
} // namespace db7::storage