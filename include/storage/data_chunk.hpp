#pragma once

#include "common.hpp"
#include "shared/align_util.hpp"
#include "shared/error/exception.hpp"

#include <cstring>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace db7 {

struct ChunkColumn {
  column_t oid;
  u16 size;

  ChunkColumn(column_t o, u16 s) : oid(o), size(s) {}
};

/**
 * [total size ib bytes] (4 bytes)
 * [column count]        (2 bytes)
 * [column ids]          (count* 2 bytes)
 * [offsets]             (count* 2 bytes)
 * [null bitmap]         (coulumn count/8 + 1 bytes)
 * [data]                (varlen)
 *
 * 1 in bitmap means value is NULL
 */
class __attribute__((packed)) DataChunk {
private:
  u32 size_;
  u16 col_count_;
  byte varlen_contents_[0];

  // Allowed creation only using casting on raw memory
  DataChunk() = delete;
  DataChunk(const DataChunk &) = delete;
  DataChunk &operator=(const DataChunk &) = delete;
  ~DataChunk() = delete;

private:
  const byte *Bytes() const { return reinterpret_cast<const byte *>(this); }

  byte *Bytes() { return reinterpret_cast<byte *>(this); }

  static constexpr u32 BitmapSize(u32 n) { return (n + 7) / 8; }

  u32 SlotSize(u16 idx) const {
    const u32 end = idx + 1 < col_count_ ? Offsets()[idx + 1] : size_;
    return end - Offsets()[idx];
  }

  u16 IdxOf(column_t oid) const {
    const column_t *cols = ColumnIds();
    for (u16 i = 0; i < col_count_; i++)
      if (cols[i] == oid) return i;
    throw std::out_of_range("column not in chunk");
  }

public:
  u32 GetSize() const { return size_; }

  u16 GetColCount() const { return col_count_; }

  const column_t *ColumnIds() const { return reinterpret_cast<const column_t *>(varlen_contents_); }

  column_t *ColumnIds() { return reinterpret_cast<column_t *>(varlen_contents_); }

  const u16 *Offsets() const { return reinterpret_cast<const u16 *>(ColumnIds() + col_count_); }

  u16 *Offsets() { return reinterpret_cast<u16 *>(ColumnIds() + col_count_); }

  const u8 *Bitmap() const { return reinterpret_cast<const u8 *>(Offsets() + col_count_); }

  u8 *Bitmap() { return const_cast<u8 *>(std::as_const(*this).Bitmap()); }

  const byte *Data() const { return Bitmap() + BitmapSize(col_count_); }

  /** Returns the size of {columns ids} + {offsets} + {column count} + {chunk size} */
  std::span<byte> GetHeaderSpan() { return std::span<byte>(Bytes(), Data() - Bytes()); }

  bool IsNull(u16 idx) const { return ((Bitmap()[idx / 8] >> (idx % 8)) & 1); }

  void SetNull(u16 idx, bool isNull) {
    const u8 mask = u8(1u << (idx % 8));
    if (isNull)
      Bitmap()[idx / 8] |= mask;
    else
      Bitmap()[idx / 8] &= u8(~mask);
  }

  byte *GetByIdx(u16 idx) { return Bytes() + Offsets()[idx]; }

  const byte *GetByIdx(u16 idx) const { return Bytes() + Offsets()[idx]; }

  byte *GetByIdxNullCheck(u16 idx) {
    if (IsNull(idx)) return nullptr;
    return GetByIdx(idx);
  }

  byte *GetByOid(column_t oid) {
    auto *cols = ColumnIds();
    for (int i = 0; i < col_count_; i++) {
      if (cols[i] == oid) { return GetByIdx(i); }
    }
    throw MEMORY_EXCEPTION("column not in chunk");
  }

  byte *GetByOidNullCheck(column_t oid) {
    auto *cols = ColumnIds();
    for (int i = 0; i < col_count_; i++) {
      if (cols[i] == oid) {
        if (IsNull(i)) return nullptr;
        return GetByIdx(i);
      }
    }
    throw MEMORY_EXCEPTION("column not in chunk");
  }

  template <typename T>
  T Get(u16 idx, bool &isNull) const {
    static_assert(std::is_trivially_copyable_v<T>);
    DB7_ASSERT(idx < col_count_, "column index out of range");
    DB7_ASSERT(sizeof(T) <= SlotSize(idx), "type larger than column slot");
    T value{};
    if (IsNull(idx)) {
      isNull = true;
    } else {
      std::memcpy(&value, GetByIdx(idx), sizeof(T));
      isNull = false;
    }
    return value;
  }

  void WriteByIdx(u16 idx, std::span<const byte> data) {
    DB7_ASSERT(data.size_bytes() <= SlotSize(idx), "Write out of bounds");
    std::memcpy(GetByIdx(idx), data.data(), data.size_bytes());
  }

  void WriteByIdxNullCheck(u16 idx, std::span<const byte> data) {
    DB7_ASSERT(data.size_bytes() <= SlotSize(idx), "Write out of bounds");
    if (data.data() == nullptr) {
      SetNull(idx, true);
      return;
    }
    std::memcpy(GetByIdx(idx), data.data(), data.size_bytes());
  }

  void WriteByOid(column_t oid, std::span<const byte> data) { WriteByIdx(IdxOf(oid), data); }

  void WriteByOidNullCheck(column_t oid, std::span<const byte> data) {
    u16 idx = IdxOf(oid);
    if (data.data() == nullptr) {
      SetNull(idx, true);
      return;
    }
    WriteByIdx(idx, data);
  }

  template <typename T>
  void Set(u16 idx, const T &value, bool isNull) {
    static_assert(std::is_trivially_copyable_v<T>);
    DB7_ASSERT(idx < col_count_, "column index out of range");
    DB7_ASSERT(sizeof(T) <= SlotSize(idx), "type larger than column slot");
    if (isNull) {
      SetNull(idx, true);
    } else {
      std::memcpy(GetByIdx(idx), &value, sizeof(T));
      SetNull(idx, false);
    }
  }

private:
  static size_t CalcTotalSizeAndOffsets(std::vector<ChunkColumn> &columns,
                                        std::vector<u16> &offsets) {
    auto column_count = columns.size();
    size_t size = sizeof(DataChunk) + sizeof(column_t) * column_count + sizeof(u16) * column_count +
                  BitmapSize(column_count);
    for (const auto &col : columns) {
      size = shared::AlignUp(size, col.size);
      DB7_ASSERT(size <= UINT16_MAX, "offset does not fit in u16");
      offsets.emplace_back(size);
      size += col.size;
    }
    return shared::AlignUp(size, sizeof(u32));
  }

public:
  static DataChunk *BuildDataChunk(std::vector<ChunkColumn> &columns) {
    std::vector<u16> offsets;
    offsets.reserve(columns.size());
    const size_t size = CalcTotalSizeAndOffsets(columns, offsets);
    DB7_ASSERT(size <= UINT32_MAX, "chunk too large");

    byte *raw = new byte[size](); // zeroed: bitmap
    auto *chunk = reinterpret_cast<DataChunk *>(raw);
    chunk->size_ = size;
    chunk->col_count_ = columns.size();

    column_t *ids = chunk->ColumnIds();
    for (size_t i = 0; i < columns.size(); i++) ids[i] = columns[i].oid;

    auto *off = chunk->Offsets();
    std::memcpy(off, offsets.data(), offsets.size() * sizeof(u16));
    return chunk;
  }

  static void Destroy(DataChunk *chunk) { delete[] reinterpret_cast<byte *>(chunk); }

  DataChunk *Copy() {
    byte *raw = new byte[size_];
    std::memcpy(raw, this, size_);
    return reinterpret_cast<DataChunk *>(raw);
  }
};

static_assert(sizeof(column_t) == 2);
static_assert(sizeof(DataChunk) == 6, "Sizes must match");
} // namespace db7