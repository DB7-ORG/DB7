#pragma once

#include "common.hpp"
#include "shared/align_util.hpp"
#include "shared/error/exception.hpp"
#include "storage/chunk_bitmap.hpp"
#include "storage/layouts/pax.hpp"

#include <cstring>
#include <memory>
#include <optional>
#include <vector>

namespace db7 {

static constexpr u32 STANDARD_VECTOR_SIZE = 2048;
static constexpr u32 CHUNK_BITMAP_BYTES = STANDARD_VECTOR_SIZE / 8;

class ColumnsChunk {
private:
  u16 count_;
  u16 col_count_;
  std::unique_ptr<byte[]> varlen_contents_;
  // deleted part
  alignas(8) u16 sel_[STANDARD_VECTOR_SIZE]; // 1 = row not visible
  u16 sel_count_ = 0;

private:
  const u32 *Offsets() const { return reinterpret_cast<const u32 *>(varlen_contents_.get()); }

  u32 *Offsets() { return reinterpret_cast<u32 *>(varlen_contents_.get()); }

  const column_t *ColumnIds() const {
    return reinterpret_cast<const column_t *>(Offsets() + col_count_);
  }

  column_t *ColumnIds() { return reinterpret_cast<column_t *>(Offsets() + col_count_); }

  const byte *Column(column_t idx) const { return varlen_contents_.get() + Offsets()[idx]; }

  byte *Column(column_t idx) { return varlen_contents_.get() + Offsets()[idx]; }

  ChunkBitmap Bitmap(column_t idx, u8 type_size) const {
    return ChunkBitmap(const_cast<byte *>(Column(idx)) + STANDARD_VECTOR_SIZE * type_size);
  }

  ChunkBitmap Bitmap(column_t idx, u8 type_size) {
    return ChunkBitmap(Column(idx) + STANDARD_VECTOR_SIZE * type_size);
  }

  u16 IdxOf(column_t oid) const {
    const column_t *cols = ColumnIds();
    for (u16 i = 0; i < col_count_; i++)
      if (cols[i] == oid) return i;
    throw std::out_of_range("column not in chunk");
  }

  std::optional<u16> IdxOfOptional(column_t oid) const {
    const column_t *cols = ColumnIds();
    for (u16 i = 0; i < col_count_; i++)
      if (cols[i] == oid) return i;
    return std::nullopt;
  }

  size_t CalcFixedSizeAndOffsets(std::vector<ChunkColumn> &columns, std::vector<u32> &offsets) {
    size_t size = sizeof(u32) * col_count_ + sizeof(column_t) * col_count_;
    for (const auto &col : columns) {
      size = shared::AlignUp(size, col.size);
      DB7_ASSERT(size <= UINT32_MAX, "offset does not fit in u32");
      offsets.emplace_back(size);
      size += col.size * STANDARD_VECTOR_SIZE + CHUNK_BITMAP_BYTES;
    }
    return size;
  }

public:
  ColumnsChunk(std::vector<ChunkColumn> &columns) : count_(0), col_count_(columns.size()) {
    std::vector<u32> offsets;
    offsets.reserve(columns.size());
    const size_t size = CalcFixedSizeAndOffsets(columns, offsets);

    varlen_contents_ = std::make_unique<byte[]>(size); // zeroed: bitmap

    column_t *ids = ColumnIds();
    for (size_t i = 0; i < columns.size(); i++) ids[i] = columns[i].oid;

    auto *off = Offsets();
    std::memcpy(off, offsets.data(), offsets.size() * sizeof(u32));
  }

  u16 GetTupleCount() const { return count_; }

  bool HasSpace() const { return GetTupleCount() < STANDARD_VECTOR_SIZE; }

  void ResetTuples() {
    count_ = 0;
    sel_count_ = 0;
  }

  void AdvanceTupleCount(u16 n) {
    count_ += n;
    DB7_ASSERT(count_ <= STANDARD_VECTOR_SIZE, "Overflow");
  }

  u16 GetColCount() const { return col_count_; }

  byte *GetColumnByIdx(u16 idx) { return varlen_contents_.get() + Offsets()[idx]; }

  const byte *GetColumnByIdx(u16 idx) const { return varlen_contents_.get() + Offsets()[idx]; }

  byte *GetColumnByOid(column_t oid) {
    auto *cols = ColumnIds();
    for (int i = 0; i < col_count_; i++) {
      if (cols[i] == oid) { return GetColumnByIdx(i); }
    }
    throw MEMORY_EXCEPTION("column not in chunk");
  }

  // TODO handle constant vectors
  /** @return Tuples appended to column in this chunk */
  void AppendColumnDataByIdx(u16 idx, storage::ColumnVector &vec, u16 start, u16 n) {
    DB7_ASSERT(vec.type_ != storage::CONSTANT, "Constant vectors not implemented");
    DB7_ASSERT(start <= vec.count_, "start past end of vector");
    DB7_ASSERT(n != 0, "No tuples in the page");

    byte *column = Column(idx);
    const auto slot_size = vec.slot_size_;

    /** Copy slots in place */
    std::memcpy(column + slot_size * count_, vec.data_ + start * slot_size, n * slot_size);

    /** Swizzle pointers for strings that are not inlined */
    if (vec.is_varlen_) {
      byte *dst = column + count_ * slot_size;
      for (u32 i = 0; i < n; i++) {
        /* Ignore null slots */
        if (vec.bitmap_ && vec.bitmap_.IsNull(start + i)) { continue; }
        auto *entry = storage::VarlenEntry::Cast(dst + size_t(i) * slot_size);
        if (entry->IsInlined()) { continue; }
        entry->Swizzle(vec.base_page_);
      }
    }

    /** Copy bitmap in place */
    auto bitmap = Bitmap(idx, slot_size);
    if (vec.bitmap_) {
      for (unsigned int i = 0; i < n; i++) {
        bitmap.SetNull(count_ + i, vec.bitmap_.IsNull(start + i));
      }
    } else {
      for (unsigned int i = 0; i < n; i++) { bitmap.SetNull(count_ + i, false); }
    }
  }

  void AppendColumnDataByOid(column_t oid, storage::ColumnVector &vec, u16 start, u16 n) {
    AppendColumnDataByIdx(IdxOf(oid), vec, start, n);
  }

  void EmplaceByIdx(u16 idx, u16 pos, byte *data, byte *base_ptr,
                    const storage::StorageColumnDesc &col) {
    DB7_ASSERT(pos < STANDARD_VECTOR_SIZE, "row out of range");
    DB7_ASSERT(data != nullptr || col.isNullable, "NULL in NOT NULL column");

    byte *column = Column(idx);
    const auto slot_size = col.slot_size;

    Bitmap(idx, col.slot_size).SetNull(pos, data == nullptr);
    if (data == nullptr) return;

    byte *slot = column + slot_size * pos;
    std::memcpy(slot, data, slot_size);

    if (col.isVarlen) {
      auto *entry = storage::VarlenEntry::Cast(slot);
      if (!entry->IsInlined()) { entry->Swizzle(base_ptr); }
    }
  }

  /** NOTE: Oid must exist in the column chunk */
  void EmplaceByOid(column_t oid, u16 pos, byte *data, byte *base_ptr,
                    const storage::StorageColumnDesc &col) {
    EmplaceByIdx(IdxOf(oid), pos, data, base_ptr, col);
  }

  void EmplaceDataChunk(u16 pos, DataChunk *chunk,
                        const std::vector<storage::StorageColumnDesc> &columns) {
    column_t *column_ids = chunk->ColumnIds();
    for (u16 i = 0; i < chunk->GetColCount(); i++) {
      column_t oid = column_ids[i];
      auto *data = chunk->GetByOidNullCheck(oid);
      auto idx = IdxOfOptional(oid);
      if (!idx) continue;
      EmplaceByIdx(*idx, pos, data, chunk->Bytes(), columns[oid]);
    }
  }

  void AppendSel(u16 row) {
    DB7_ASSERT(sel_count_ < STANDARD_VECTOR_SIZE, "Selection vector full");
    DB7_ASSERT(row < count_, "Row outside chunk");
    DB7_ASSERT(sel_count_ == 0 || sel_[sel_count_ - 1] < row, "Selection must be ascending");
    sel_[sel_count_++] = row;
  }
};
} // namespace db7