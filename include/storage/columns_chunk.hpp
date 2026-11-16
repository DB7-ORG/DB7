#pragma once

#include "common.hpp"
#include "shared/align_util.hpp"
#include "shared/error/exception.hpp"
#include "storage/chunk_bitmap.hpp"

#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace db7 {

static constexpr u32 STANDARD_VECTOR_SIZE = 2048;
static constexpr u32 CHUNK_BITMAP_BYTES = STANDARD_VECTOR_SIZE / 8;

class ColumnsChunk {
private:
  u16 count_;
  u16 col_count_;
  std::unique_ptr<byte[]> varlen_contents_;

private:
  const u32 *Offsets() const { return reinterpret_cast<const u32 *>(varlen_contents_.get()); }

  u32 *Offsets() { return reinterpret_cast<u32 *>(varlen_contents_.get()); }

  const column_t *ColumnIds() const {
    return reinterpret_cast<const column_t *>(Offsets() + col_count_);
  }

  column_t *ColumnIds() { return reinterpret_cast<column_t *>(Offsets() + col_count_); }

  const byte *Column(column_t idx) const { return varlen_contents_.get() + Offsets()[idx]; }

  byte *Column(column_t idx) { return varlen_contents_.get() + Offsets()[idx]; }

  const ChunkBitmap Bitmap(column_t idx, u8 type_size) const {
    return ChunkBitmap(Column(idx) + STANDARD_VECTOR_SIZE * type_size);
  }

  ChunkBitmap Bitmap(column_t idx, u8 type_size) {
    return ChunkBitmap(std::as_const(*this).Bitmap(idx, type_size));
  }

  u16 IdxOf(column_t oid) const {
    const column_t *cols = ColumnIds();
    for (u16 i = 0; i < col_count_; i++)
      if (cols[i] == oid) return i;
    throw std::out_of_range("column not in chunk");
  }

  size_t CalcFixedSizeAndOffsets(std::vector<ChunkColumn> &columns, std::vector<u32> &offsets) {
    size_t size = sizeof(u32) * col_count_ + sizeof(column_t) * col_count_;
    for (const auto &col : columns) {
      size = shared::AlignUp(size, col.size);
      DB7_ASSERT(size <= UINT32_MAX, "offset does not fit in u16");
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

  void WriteColumnByIdx(u16 idx, u8 slot_size, std::span<const byte> data, const byte *src_bitmap,
                        u16 count) {
    const size_t used = size_t(count_) * slot_size;
    DB7_ASSERT(data.size_bytes() % slot_size == 0, "partial slot");
    DB7_ASSERT(used + data.size_bytes() <= size_t(STANDARD_VECTOR_SIZE) * slot_size,
               "column overflow");
    if (count == 0) return;

    byte *col = GetColumnByIdx(idx);
    std::memcpy(col + used, data.data(), data.size_bytes());

    // bitmap: lives after the values, bit i = byte i/8, bit i%8
    u8 *d = reinterpret_cast<u8 *>(col + size_t(STANDARD_VECTOR_SIZE) * slot_size) + count_ / 8;
    const u8 *s = reinterpret_cast<const u8 *>(src_bitmap);
    const u32 shift = count_ % 8;
    const u32 src_bytes = (u32(count) + 7) / 8;

    if (shift == 0) {
      std::memcpy(d, s, src_bytes);
    } else {
      d[0] = u8((d[0] & u8((1u << shift) - 1)) | u8(s[0] << shift)); // keep existing low bits
      for (u32 i = 1; i < src_bytes; i++) d[i] = u8((s[i - 1] >> (8 - shift)) | u8(s[i] << shift));
      if (shift + count > src_bytes * 8) // tail spills into one more byte
        d[src_bytes] = u8(s[src_bytes - 1] >> (8 - shift));
    }

    count_ += count;
  }

  void WriteColumnByOid(column_t oid, u8 slot_size, std::span<const byte> data,
                        const byte *src_bitmap, u16 count) {
    WriteColumnByIdx(IdxOf(oid), slot_size, data, src_bitmap, count);
  }
};
} // namespace db7