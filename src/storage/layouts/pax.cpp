#include "storage/layouts/pax.hpp"

namespace db7::storage {
size_t Pax::CalculateNumTuples(size_t row_heap_size, size_t row_size, size_t padding,
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

u16 Pax::AppendHeapUnsafe(byte *page, std::span<const byte> ptr) {
  u16 curr_heap_offset = PaxHeader::CurrHeapOffset(page);
  const u16 new_heap = curr_heap_offset - ptr.size_bytes();
  std::memcpy(page + new_heap, ptr.data(), ptr.size_bytes());
  PaxHeader::SetCurrHeapOffset(page, new_heap);
  return new_heap;
}

u16 Pax::CalcHeapBytes(u16 size) {
  return size > Pax::varlen_size ? size - (Pax::varlen_size - sizeof(u16)) : 0;
}

u16 Pax::CalcHeapBytes(type_id type, u16 size) {
  return (IsVarlen(type) && size > Pax::varlen_size) ? size - (Pax::varlen_size - sizeof(u16)) : 0;
}

byte *Pax::ColBitmap(byte *page, u16 offset, u8 type_size, u16 capacity) {
  return page + offset + capacity * type_size;
}

/* {0 -> not null , 1 -> null} */
bool Pax::IsNull(byte *bitmap, u16 idx) {
  byte mask = 1 << (idx % 8);
  return bitmap[idx / 8] & mask;
}

template <bool Null>
void Pax::SetBit(byte *bitmap, u16 idx) {
  byte mask = 1 << (idx % 8);
  u16 pos = idx / 8;
  if constexpr (Null) {
    bitmap[pos] |= mask;
  } else {
    bitmap[pos] &= ~mask;
  }
}

u16 Pax::StripOff(u16 offset) {
  return static_cast<u16>(offset & ~PaxHeader::offset_nullable_mask_);
}

size_t Pax::CalcHeapBytes(std::vector<PaxInsertInfo> &columns) {
  size_t total = 0;
  for (const auto &col : columns) { total += CalcHeapBytes(col.type, col.size); }
  return total;
}

// void Pax::AppendVarlenUnsafe(byte *page, byte *dest, VarlenEntry *entry) {
//   /* Appends first part of the slot [len] [inline data] [heap offset(empty)] */
//   std::memcpy(dest, payload.data(), inline_ptr_len);

//   /* Appends to the heap area */
//   u16 heap_ptr_len = payload.size() - inline_ptr_len;
//   byte *heap_ptr = payload.data() + inline_ptr_len;
//   u16 off = AppendHeapUnsafe(page, {heap_ptr, heap_ptr_len});

//   /* Appends the offset [len] [inline data] [heap offset] */
//   std::memcpy(dest + inline_ptr_len, &off, sizeof(u16));
// }

// void Pax::ReadVarlenUnsafe(const byte *page, const byte *slot, byte *dest) {
//   // TODO storage should be fixed for new varlen
//   auto *entry = reinterpret_cast<const VarlenEntry *>(slot);
//   if (entry->IsInlined()) {
//     std::memcpy(dest, slot, entry->Size());
//   } else {
//     std::memcpy(dest, slot, inline_ptr_len);
//     std::memcpy(dest + inline_ptr_len, page + entry->Offset(), entry->Size() - inline_ptr_len);
//   }
// }

void Pax::Init(byte *page, std::vector<ColumnStatistics> &stats) {
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
  size_t N =
      CalculateNumTuples(row_heap_size, row_size, worst_case_padding, stats.size(), nullable_count);
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

byte *CalcCellPos(byte *page, u16 stripped_col_offset, u16 row_idx, u16 slot_size) {
  return page + stripped_col_offset + row_idx * slot_size;
}

PaxInsertResponse Pax::Insert(byte *page, DataChunk *chunk, std::vector<PaxInsertInfo> &columns,
                              size_t required_heap, column_t catalog_col_count) {
  const column_t ncols = PaxHeader::ColumnCount(page);
  if (catalog_col_count < ncols) { return PaxInsertResponse::WRITER_SCHEMA_STALE; }
  if (catalog_col_count > ncols) { return PaxInsertResponse::PAGE_SCHEMA_OLDER; }

  if (required_heap > PaxHeader::CurrHeapOffset(page) - PaxHeader::MaxHeapOffset(page))
    return PaxInsertResponse::NO_SPACE_HEAP;

  /** Is there still space in the page */
  const u16 ntup = PaxHeader::TupleCount(page);
  const u16 capacity = PaxHeader::Capacity(page);
  if (ntup >= capacity) { return PaxInsertResponse::NO_SPACE_CAPACITY; }

  /** Get offset array from page and column_ids array from chunk */
  const u16 *offsets = PaxHeader::Offsets(page);
  const column_t *column_ids = chunk->ColumnIds();

  /**
   * Validate before writing anything, so a refused insert leaves the page untouched.
   * A NULL for a column without a bitmap means the page was written before the column
   * became nullable (or the column is NOT NULL). Treat it like a full page so the
   * caller moves on (e.g. zero space hint in the FSM).
   */
  for (u16 i = 0; i < chunk->GetColCount(); ++i) {
    const column_t col_id = column_ids[i];
    DB7_ASSERT(col_id < ncols, "Column tried to write outside of range");

    const u16 offset = offsets[col_id];
    // if (offset == Pax::invalid_offset) { return PaxInsertResponse::WRITER_SCHEMA_STALE; }
    DB7_ASSERT(offset != Pax::invalid_offset, "trying to insert to dropped column");
    // if (offset == Pax::invalid_offset) continue;
    if (chunk->IsNull(i) && !(offset & PaxHeader::offset_nullable_mask_))
      return PaxInsertResponse::PAGE_SCHEMA_OLDER;
  }

  /** Scan over every column in chunk */
  for (u16 i = 0; i < chunk->GetColCount(); ++i) {
    column_t col_id = column_ids[i];
    DB7_ASSERT(col_id < ncols, "Column tried to write outside of range");

    /** // TODO storage
     * If offset is invalid that means this column was deleted ignore it
     * should probably consider aborting since the catalog changed the table schema
     * and we still have the stale data
     */
    const u16 offset = offsets[col_id];
    // if (offset == Pax::invalid_offset) continue;

    const auto column = columns[i];
    /** Get data from the chunk that will be copied to the page */
    byte *ptr = chunk->GetByIdxNullCheck(i);

    const u16 stripped_offset = StripOff(offset);

    if (offset & PaxHeader::offset_nullable_mask_) {
      byte *bitmap = ColBitmap(page, stripped_offset, column.slot_size, capacity);
      if (ptr == nullptr) {
        SetBit<true>(bitmap, ntup);
        continue;
      } else {
        SetBit<false>(bitmap, ntup);
      }
    }
    DB7_ASSERT(ptr != nullptr, "Data cant be null here");
    // TODO this should be unreachable since we have a check already above it
    // else if (ptr == nullptr) {
    //   /* if bitmap doesnt exist for this column but in chunk we get nullptr (value is null) this
    //    * is likely an old page. Treat it as if there is no space in the page and make sure to
    //    mark
    //    * it in the FSM as zero space hint. */
    //   return false;
    // }

    byte *dest = CalcCellPos(page, stripped_offset, ntup, column.slot_size);
    if (!IsVarlen(column.type)) { // should be based on type
      DB7_ASSERT(column.size == column.slot_size, "fixed-width value size mismatch");
      std::memcpy(dest, ptr, column.size);
    } else {
      const auto *entry = VarlenEntry::Cast(ptr);
      std::memcpy(dest, entry, sizeof(VarlenEntry));
      if (!entry->IsInlined()) {
        u16 off = AppendHeapUnsafe(page, entry->Heap(chunk->Bytes()));
        VarlenEntry::Cast(dest)->SetOffset(off);
      }
    }
  }

  PaxHeader::SetTupleCount(page, ntup + 1);

  return PaxInsertResponse::OK;
}

bool Pax::IsPageVisible(byte *page, const std::vector<PaxGetInfo> &cols,
                        column_t catalog_col_count) {
  const column_t ncols = PaxHeader::ColumnCount(page);
  if (ncols > catalog_col_count)
    return false; // created under a newer txn that called ADD COLUMN: optional fast skip
  const u16 *offsets = PaxHeader::Offsets(page);
  for (const auto &c : cols) {
    if (c.column_id >= ncols) continue;
    if (offsets[c.column_id] == Pax::invalid_offset)
      return false; // created after a DROP this reader doesn't see
  }
  return true;
}

DataChunk *Pax::Get(byte *page, u16 row_idx, std::vector<PaxGetInfo> &columns,
                    u16 catalog_col_count, DataChunk *initializer) {

  DB7_ASSERT(row_idx < PaxHeader::TupleCount(page), "Row outside the inserted tuples");
  DB7_ASSERT(IsPageVisible(page, columns, catalog_col_count),
             "IsPageVisible Should be called before this to see if page is valid");

  const u16 capacity = PaxHeader::Capacity(page);
  const u16 *offsets = PaxHeader::Offsets(page);
  const column_t ncols = PaxHeader::ColumnCount(page);

  u16 alloc_size = 0;
  for (size_t i = 0; i < columns.size(); i++) {

    const auto &col = columns[i];
    DB7_ASSERT(col.isVarlen || col.size == col.slot_size,
               "Slot size and size must be equal for fixed types");

    if (col.isVarlen) {
      if (col.column_id >= ncols) {
        if (col.size > varlen_size && col.default_ptr != nullptr) {
          alloc_size += col.size - inline_ptr_len;
        }
        continue;
      }

      const auto offset = offsets[col.column_id];
      const auto stripped = StripOff(offset);

      if (offset & PaxHeader::offset_nullable_mask_) {
        byte *bitmap = ColBitmap(page, stripped, col.slot_size, capacity);
        if (IsNull(bitmap, row_idx)) { continue; }
      }

      byte *pos = CalcCellPos(page, stripped, row_idx, col.slot_size);
      auto *varlen = VarlenEntry::Cast(pos);
      if (!varlen->IsInlined()) { alloc_size += varlen->HeapSize(); }
    }
  }

  // Builds data chunk
  auto *chunk = DataChunk::BuildDataChunk(initializer, alloc_size);

  for (size_t i = 0; i < columns.size(); i++) {
    const auto &col = columns[i];

    byte *dest = chunk->GetByIdx(i);

    if (col.column_id >= ncols) {
      if (col.default_ptr == nullptr) {
        chunk->SetNull(i, true);
      } else if (col.isVarlen) {
        const bool spills = col.size > varlen_size;
        const size_t s = spills ? varlen_size : col.size;
        std::span<const byte> slot{col.default_ptr, s};
        std::span<const byte> heap = spills
                                         ? std::span<const byte>{col.default_ptr + inline_ptr_len,
                                                                 size_t(col.size - inline_ptr_len)}
                                         : std::span<const byte>{};
        chunk->WriteVarlen(i, slot, heap); // overwrites bytes 14..15 with the chunk offset
      } else {
        std::memcpy(dest, col.default_ptr, col.size);
      }
      continue;
    }

    const auto offset = offsets[col.column_id];

    const auto stripped = StripOff(offset);

    if (offset & PaxHeader::offset_nullable_mask_) {
      byte *bitmap = ColBitmap(page, stripped, col.slot_size, capacity);
      if (IsNull(bitmap, row_idx)) {
        chunk->SetNull(i, true);
        continue;
      }
    }

    byte *pos = CalcCellPos(page, stripped, row_idx, col.slot_size);
    if (col.isVarlen) {
      // ReadVarlenUnsafe(page, pos, dest);
      auto *entry = VarlenEntry::Cast(pos);
      const auto heap = entry->IsInlined() ? std::span<byte>() : entry->Heap(page);
      chunk->WriteVarlen(i, entry->Slot(), heap);
    } else {
      std::memcpy(dest, pos, col.slot_size);
    }
  }
  return chunk;
}

} // namespace db7::storage