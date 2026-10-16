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
}

u16 Pax::CalcHeapBytes(u16 size) { return VarlenEntry::HeapSizeFor(size); }

u16 Pax::CalcHeapBytes(type_id type, u16 size) {
  if (!IsVarlen(type)) { return 0; }
  return CalcHeapBytes(size);
}

size_t Pax::CalcHeapBytes(const std::vector<storage::StorageColumnDesc> &columns,
                          const DataChunk *chunk) {
  u16 required_heap = 0;
  const column_t *chunk_column_ids = chunk->ColumnIds();
  for (u16 i = 0; i < chunk->GetColCount(); i++) {
    const auto col = columns[chunk_column_ids[i]];
    DB7_ASSERT(!col.isDropped, "Tried to index into dropped columns");
    if (col.isVarlen) {
      const auto *slot = chunk->GetByIdxNullCheck(i);
      if (slot == nullptr) { continue; }
      const storage::VarlenEntry *var_ptr = storage::VarlenEntry::Cast(slot);
      required_heap += var_ptr->HeapSize();
    }
  }
  return required_heap;
}

u16 Pax::MaxRowCount(byte *page) { return PaxHeader::Capacity(page); }

u16 Pax::CurrentRowCount(byte *page) { return PaxHeader::TupleCount(page); }

byte *Pax::CalcCellPos(byte *page, u16 stripped_col_offset, u16 row_idx, u16 slot_size) {
  return page + stripped_col_offset + row_idx * slot_size;
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

// TODO storage could add min heap size if i need some specific layout
// for a really large string for example so i dont get stuck creating
// pages that dont have enough heap
void Pax::Init(byte *page, std::vector<StorageColumnDesc> &columns, u16 avg_heap_size,
               size_t min_heap_size) {
  DB7_ASSERT(min_heap_size <= UINT16_MAX, "Out of bounds");
  DB7_ASSERT(columns.size() >= 1, "number of columns in the page must be atleast 1");
  PaxHeader::SetMagic(page);
  PaxHeader::SetColumnCount(page, columns.size());
  PaxHeader::SetTupleCount(page, 0);
  PaxHeader::SetCurrHeapOffset(page, DB7_PAGE_SIZE);

  /** Calculates cost of each row(size) for regular minipage and heap */
  size_t row_heap_size = avg_heap_size;
  size_t row_size = 0;
  size_t valid_column_count = 0;
  size_t nullable_count = 0;
  for (const auto &s : columns) {
    if (s.isDropped) continue;
    // if (IsVarlen(s.type)) { row_heap_size += CalcHeapBytes(s.avg_size); }
    row_size += SizeOf(s.type);
    valid_column_count++;
    nullable_count += size_t(s.isNullable);
  }
  row_heap_size = std::min(row_heap_size, min_heap_size);
  DB7_ASSERT(row_size > 0, "table has no live columns");
  DB7_ASSERT(valid_column_count >= 1, "number of valid columns in the page must be atleast 1");

  /**
   * Worst case padding for INT is 4-1 bytes.
   * With that logic worst case padding for an entire
   * row is row_size - number of columns
   */
  auto worst_case_padding = row_size - valid_column_count;
  size_t N = CalculateNumTuples(row_heap_size, row_size, worst_case_padding, columns.size(),
                                nullable_count);
  PaxHeader::SetCapacity(page, N);

  u16 *offsets = PaxHeader::Offsets(page);
  size_t start = columns.size() * sizeof(u16) + PaxHeader::fixed_header_size_;
  int i = 0;
  for (const auto &s : columns) {
    if (s.isDropped) {
      offsets[i] = Pax::invalid_offset;
    } else {
      const auto col_size = SizeOf(s.type);
      start = shared::AlignUp(start, col_size);
      u16 offset_nullable = s.isNullable ? PaxHeader::offset_nullable_mask_ : 0;
      offsets[i] = static_cast<u16>(start) | offset_nullable;
      const auto null_bitmap_size = s.isNullable ? (N + 7) / 8 : 0;
      start += N * col_size + null_bitmap_size;
    }
    i++;
  }
  DB7_ASSERT(start + N * row_heap_size <= DB7_PAGE_SIZE, "layout overflows page");
  PaxHeader::SetMaxHeapOffset(page, start);
}

u16 Pax::AppendHeapUnsafe(byte *page, std::span<const byte> ptr) {
  u16 curr_heap_offset = PaxHeader::CurrHeapOffset(page);
  const u16 new_heap = curr_heap_offset - ptr.size();
  std::memcpy(page + new_heap, ptr.data(), ptr.size());
  PaxHeader::SetCurrHeapOffset(page, new_heap);
  return new_heap;
}

void Pax::WriteSlot(byte *page, byte *dest, byte *source, const StorageColumnDesc &column,
                    const DataChunk *chunk) {
  if (!IsVarlen(column.type)) {
    DB7_ASSERT(column.default_val.size() == 0 || column.default_val.size() == column.slot_size,
               "fixed-width value size mismatch");
    std::memcpy(dest, source, column.slot_size);
  } else {
    const auto *entry = VarlenEntry::Cast(source);
    std::memcpy(dest, entry, sizeof(VarlenEntry));
    if (!entry->IsInlined()) {
      u16 off = AppendHeapUnsafe(page, entry->Heap(chunk->Bytes()));
      VarlenEntry::Cast(dest)->SetOffset(off);
    }
  }
}

PaxInsertResponse Pax::Insert(byte *page, DataChunk *chunk, std::vector<StorageColumnDesc> &columns,
                              size_t required_heap, column_t catalog_col_count) {
  DB7_ASSERT(chunk->GetColCount() == columns.size(), "Mismatch in DataTable and chunk sizes");

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
  // TODO storage this should be done once per page
  for (u16 i = 0; i < chunk->GetColCount(); ++i) {
    const column_t col_id = column_ids[i];
    DB7_ASSERT(col_id < ncols, "Column tried to write outside of range");

    const u16 offset = offsets[col_id];
    if (offset == Pax::invalid_offset) { return PaxInsertResponse::WRITER_SCHEMA_STALE; }
    DB7_ASSERT(offset != Pax::invalid_offset, "trying to insert to dropped column");
    if (chunk->IsNull(i) && !(offset & PaxHeader::offset_nullable_mask_))
      return PaxInsertResponse::PAGE_SCHEMA_OLDER;
  }

  /** Scan over every column in chunk */
  for (u16 i = 0; i < chunk->GetColCount(); ++i) {
    column_t col_id = column_ids[i];
    DB7_ASSERT(col_id < ncols, "Column tried to write outside of range");

    const u16 offset = offsets[col_id];

    const auto column = columns[i];
    DB7_ASSERT(!column.isDropped, "Dropped columns shouldnt be included");
    /** Get data from the chunk that will be copied to the page */
    byte *ptr = chunk->GetByIdxNullCheck(i);

    const u16 stripped_offset = StripOff(offset);

    /* Set null bitmap */
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

    /* Copy data to the right cell */
    byte *dest = CalcCellPos(page, stripped_offset, ntup, column.slot_size);
    WriteSlot(page, dest, ptr, column, chunk);
  }

  PaxHeader::SetTupleCount(page, ntup + 1);

  return PaxInsertResponse::OK;
}

bool Pax::IsPageVisible(byte *page, const std::vector<StorageColumnDesc> &cols,
                        column_t catalog_col_count) {
  const column_t ncols = PaxHeader::ColumnCount(page);
  if (ncols > catalog_col_count)
    return false; // created under a newer txn that called ADD COLUMN: optional fast skip
  const u16 *offsets = PaxHeader::Offsets(page);
  for (const auto &c : cols) {
    if (c.column_id >= ncols)
      continue; // ur schema contains new columns, but ur still able to read existing pages by
                // setting the default value for new columns
    if (offsets[c.column_id] == Pax::invalid_offset)
      return false; // created after a DROP this reader doesn't see: optional fast skip
  }
  return true;
}

u32 Pax::CalculateChunkHeapSize(byte *page, const column_t ncols, const u16 capacity,
                                const u16 *offsets, const u16 row_idx,
                                std::vector<StorageColumnDesc> &columns) {
  u32 alloc_size = 0;
  for (size_t i = 0; i < columns.size(); i++) {

    const auto &col = columns[i];
    DB7_ASSERT(col.isVarlen || col.default_val.size() == col.slot_size,
               "Slot size and size must be equal for fixed types");

    if (col.isVarlen) {
      if (col.column_id >= ncols) {
        // checks if columns that dont exist in the page layout contibute
        // to allocating heap for the chunk. Columns that do are varlen,
        // have id bigger than ncols and are not null by default
        alloc_size += CalcHeapBytes(col.default_val.size());
        continue;
      }

      const auto offset = offsets[col.column_id];
      const auto stripped = StripOff(offset);

      if (offset & PaxHeader::offset_nullable_mask_) {
        // if column contains a bitmap and is null it doesnt contribute to heap size
        byte *bitmap = ColBitmap(page, stripped, col.slot_size, capacity);
        if (IsNull(bitmap, row_idx)) { continue; }
      }

      byte *pos = CalcCellPos(page, stripped, row_idx, col.slot_size);
      auto *varlen = VarlenEntry::Cast(pos);
      alloc_size += varlen->HeapSize();
    }
  }

  return alloc_size;
}

void Pax::WriteDefaultValueToChunk(byte *dest, const StorageColumnDesc &col, DataChunk *chunk,
                                   const size_t i) {
  const auto default_ptr = col.default_val.data();
  const auto size = col.default_val.size();
  if (default_ptr == nullptr) {
    chunk->SetNull(i, true);
  } else if (col.isVarlen) {
    chunk->WriteVarlenByIdx(i, col.default_val);
  } else {
    std::memcpy(dest, default_ptr, size);
  }
}

DataChunk *Pax::Get(byte *page, u16 row_idx, std::vector<StorageColumnDesc> &columns,
                    u16 catalog_col_count, DataChunk *initializer) {

  DB7_ASSERT(row_idx < PaxHeader::TupleCount(page), "Row outside the inserted tuples");
  DB7_ASSERT(IsPageVisible(page, columns, catalog_col_count),
             "IsPageVisible Should be called before this to see if page is valid");

  const u16 capacity = PaxHeader::Capacity(page);
  const u16 *offsets = PaxHeader::Offsets(page);
  const column_t ncols = PaxHeader::ColumnCount(page);

  // Calculates the size of the heap in data chunk for this data
  const u32 alloc_size = CalculateChunkHeapSize(page, ncols, capacity, offsets, row_idx, columns);

  // Builds data chunk
  auto *chunk = DataChunk::BuildDataChunk(initializer, alloc_size);

  for (size_t i = 0; i < columns.size(); i++) {
    const auto &col = columns[i];

    byte *dest = chunk->GetByIdx(i);

    if (col.column_id >= ncols) {
      // Writes a default value into the chunk since the column doesnt exist in layout
      WriteDefaultValueToChunk(dest, col, chunk, i);
      continue;
    }

    const auto offset = offsets[col.column_id];

    const auto stripped = StripOff(offset);

    if (offset & PaxHeader::offset_nullable_mask_) {
      // Reads a bitmap value in case of null and copies it to chunk bitmap
      byte *bitmap = ColBitmap(page, stripped, col.slot_size, capacity);
      if (IsNull(bitmap, row_idx)) {
        chunk->SetNull(i, true);
        continue;
      }
    }

    byte *cell = CalcCellPos(page, stripped, row_idx, col.slot_size);
    if (col.isVarlen) {
      auto *entry = VarlenEntry::Cast(cell);
      chunk->WriteVarlenByIdx(i, entry->Data(page));
    } else {
      std::memcpy(dest, cell, col.slot_size);
    }
  }
  return chunk;
}

} // namespace db7::storage