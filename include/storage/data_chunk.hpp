#pragma once

#include "common.hpp"
#include "shared/align_util.hpp"
#include "shared/data_structures/bitset.hpp"
#include "shared/error/exception.hpp"
#include "storage/layouts/storage_column_desc.hpp"
#include "storage/varlen_entry.hpp"

#include <cstring>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace db7 {

struct ChunkColumn {
  // column id
  column_t oid;
  // size of fixed part (pow of 2).
  // NOTE: for warlen is 16 bytes not size of payload
  u16 size;

  ChunkColumn(column_t o, u16 s) : oid(o), size(s) {}
};
// TODO storage shouldnt fill the chunk by copying
// instead copy header and the rest should be filled using an iterator or something like that
/**
 * [total size ib bytes] (4 bytes)
 * [column count]        (2 bytes)
 * [column ids]          (count* 2 bytes)
 * [offsets]             (count* 2 bytes)
 * [null bitmap]         (coulumn count/8 + 1 bytes)
 * [data slots]          (fixed len)
 * [growing heap]        (varlen data, heap is growing down)
 *
 * 1 in bitmap means value is NULL
 */
class __attribute__((packed)) DataChunk {
private:
  u32 size_;                // size of the block also used for tracking growing heap position
  u16 col_count_;           // number of columns
  u16 fixed_size_;          // header size which is fixed and doesnt change
  byte varlen_contents_[0]; // data slots and growing heap area

  // Allowed creation only using casting on raw memory
  DataChunk() = delete;
  DataChunk(const DataChunk &) = delete;
  DataChunk &operator=(const DataChunk &) = delete;
  ~DataChunk() = delete;

private:
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

  const u16 *Offsets() const { return reinterpret_cast<const u16 *>(ColumnIds() + col_count_); }

  u16 *Offsets() { return reinterpret_cast<u16 *>(ColumnIds() + col_count_); }

  const u8 *Bitmap() const { return reinterpret_cast<const u8 *>(Offsets() + col_count_); }

  u8 *Bitmap() { return const_cast<u8 *>(std::as_const(*this).Bitmap()); }

  const byte *DataSlots() const { return Bitmap() + BitmapSize(col_count_); }

  const byte *Heap() const { return Bytes() + size_; } // TODO storage check

  u16 AppendTail(const byte *data, u16 len) {
    DB7_ASSERT(size_ + len <= UINT16_MAX, "varlen offsets are u16");
    const u16 off = static_cast<u16>(size_);
    size_ += len;
    std::memcpy(Bytes() + off, data, len);
    return off;
  }

public:
  static void Destroy(DataChunk *chunk) { delete[] reinterpret_cast<byte *>(chunk); }

  byte *Bytes() { return reinterpret_cast<byte *>(this); }

  const byte *Bytes() const { return reinterpret_cast<const byte *>(this); }

  std::span<byte> Span() { return {Bytes(), GetSize()}; }

  u32 GetSize() const { return size_; }

  u16 GetColCount() const { return col_count_; }

  u16 GetHeapSize() const {
    DB7_ASSERT(size_ >= fixed_size_, "Corrupted chunk");
    return size_ - fixed_size_;
  }

  const column_t *ColumnIds() const { return reinterpret_cast<const column_t *>(varlen_contents_); }

  column_t *ColumnIds() { return reinterpret_cast<column_t *>(varlen_contents_); }

  /** Returns the header size of {columns ids} + {offsets} + {column count} + {chunk size} */
  std::span<byte> GetHeaderSpan() { return std::span<byte>(Bytes(), DataSlots() - Bytes()); }

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

  const byte *GetByIdxNullCheck(u16 idx) const {
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

  // template <typename T>
  // T Get(u16 idx, bool &isNull) const { // TODO storage doesnt work for varlen
  //   static_assert(std::is_trivially_copyable_v<T>);
  //   DB7_ASSERT(idx < col_count_, "column index out of range");
  //   DB7_ASSERT(sizeof(T) <= SlotSize(idx), "type larger than column slot");
  //   T value{};
  //   if (IsNull(idx)) {
  //     isNull = true;
  //   } else {
  //     std::memcpy(&value, GetByIdx(idx), sizeof(T));
  //     isNull = false;
  //   }
  //   return value;
  // }

  void WriteByIdx(u16 idx, std::span<const byte> data) {
    DB7_ASSERT(data.size_bytes() <= SlotSize(idx), "Write out of bounds");
    std::memcpy(GetByIdx(idx), data.data(), data.size_bytes());
  }

  void WriteByIdxNullCheck(u16 idx, std::span<const byte> data) {
    DB7_ASSERT(data.size_bytes() <= SlotSize(idx), "Write out of bounds");
    if (data.data() == nullptr) {
      SetNull(idx, true);
      return;
    } else {
      SetNull(idx, false);
    }
    std::memcpy(GetByIdx(idx), data.data(), data.size_bytes());
  }

  void WriteByOid(column_t oid, std::span<const byte> data) { WriteByIdx(IdxOf(oid), data); }

  void WriteByOidNullCheck(column_t oid, std::span<const byte> data) {
    u16 idx = IdxOf(oid);
    if (data.data() == nullptr) {
      SetNull(idx, true);
      return;
    } else {
      SetNull(idx, false);
    }
    WriteByIdx(idx, data);
  }

  // template <typename T> // TODO storage doesnt work for varlen
  // void Set(u16 idx, const T &value, bool isNull) {
  //   static_assert(std::is_trivially_copyable_v<T>);
  //   DB7_ASSERT(idx < col_count_, "column index out of range");
  //   DB7_ASSERT(sizeof(T) <= SlotSize(idx), "type larger than column slot");

  //   if (isNull) {
  //     SetNull(idx, true);
  //   } else {
  //     std::memcpy(GetByIdx(idx), &value, sizeof(T));
  //     SetNull(idx, false);
  //   }
  // }

  void WriteVarlenByIdx(u16 idx, std::span<const byte> payload) {
    using storage::VarlenEntry;
    DB7_ASSERT(idx < col_count_, "column index out of range");
    DB7_ASSERT(Offsets()[idx] + VarlenEntry::slot_size <= fixed_size_, "slot outside fixed area");
    DB7_ASSERT(payload.size() <= UINT16_MAX, "varlen length does not fit in u16");

    // insert prefix
    byte *dest = GetByIdx(idx);
    *reinterpret_cast<u16 *>(dest) = payload.size();
    std::memcpy(dest + sizeof(u16), payload.data(),
                std::min(payload.size(), size_t(VarlenEntry::inline_bytes)));

    // check if i need to insert something to heap
    auto *entry = VarlenEntry::Cast(dest);
    if (entry->IsInlined()) { return; }

    // insert to heap
    DB7_ASSERT(entry->HeapSize() == payload.size(),
               "spilled value must store the whole payload in heap");

    const u16 off = AppendTail(payload.data(), static_cast<u16>(payload.size()));
    entry->SetOffset(off);
  }

  void WriteVarlenByOid(column_t oid, std::span<const byte> payload) {
    WriteVarlenByIdx(IdxOf(oid), payload);
  }

  void WriteVarlenByIdxNullCheck(u16 idx, std::span<const byte> payload) {
    if (payload.data() == nullptr) {
      SetNull(idx, true);
      return;
    } else {
      SetNull(idx, false);
    }
    WriteVarlenByIdx(idx, payload);
  }

  void WriteVarlenByOidNullCheck(column_t oid, std::span<const byte> payload) {
    WriteVarlenByIdxNullCheck(IdxOf(oid), payload);
  }

private:
  static size_t CalcFixedSizeAndOffsets(std::vector<ChunkColumn> &columns,
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
    return size;
  }

  DataChunk *Allocate(u32 heap_size) const {
    byte *raw = new byte[fixed_size_ + heap_size];
    std::memcpy(raw, this, size_t(DataSlots() - Bytes()));
    return reinterpret_cast<DataChunk *>(raw);
  }

public:
  static DataChunk *BuildDataChunkInitializer(std::vector<storage::StorageColumnDesc> &columns) {
    std::vector<ChunkColumn> cols;
    cols.reserve(columns.size());
    for (const auto &c : columns) { cols.emplace_back(ChunkColumn{c.column_id, c.slot_size}); }
    return BuildDataChunkInitializer(cols);
  }

  static DataChunk *BuildDataChunkInitializer(std::vector<ChunkColumn> &columns) {
    std::vector<u16> offsets;
    offsets.reserve(columns.size());
    const size_t size = CalcFixedSizeAndOffsets(columns, offsets);

    byte *raw = new byte[size](); // zeroed: bitmap
    auto *chunk = reinterpret_cast<DataChunk *>(raw);
    chunk->size_ = size; // only for initializer
    chunk->fixed_size_ = size;
    chunk->col_count_ = columns.size();

    column_t *ids = chunk->ColumnIds();
    for (size_t i = 0; i < columns.size(); i++) ids[i] = columns[i].oid;

    auto *off = chunk->Offsets();
    std::memcpy(off, offsets.data(), offsets.size() * sizeof(u16));
    return chunk;
  }

  static DataChunk *BuildDataChunk(const DataChunk *initializer, u32 size) {
    return initializer->Allocate(size);
  }

  DataChunk *Copy() {
    byte *raw = new byte[size_];
    std::memcpy(raw, this, size_);
    return reinterpret_cast<DataChunk *>(raw);
  }

  /* columns is all columns in the catalog including dropped ones */
  void MergeInto(DataChunk *in, const std::vector<storage::StorageColumnDesc> &main_columns) {
    DB7_ASSERT(this->GetColCount() >= in->GetColCount(), "Out should have all of in columns");
    Db7AssertMergeColumnOrder(this, in);

    const column_t *in_cols = in->ColumnIds();
    for (u16 i = 0; i < in->GetColCount(); i++) {
      const auto &column = main_columns[in_cols[i]];
      byte *data = in->GetByIdxNullCheck(i);
      u16 idx = IdxOf(column.column_id);

      if (data == nullptr) {
        SetNull(idx, true);
        continue;
      } else {
        SetNull(idx, false);
      }

      if (column.isVarlen) {
        auto varlen = storage::VarlenEntry::Cast(data);
        this->WriteVarlenByOid(column.column_id, varlen->Data(in->Bytes()));
      } else {
        this->WriteByOid(column.column_id, {data, column.slot_size});
      }
    }
  }

private:
  void Db7AssertMergeColumnOrder(DataChunk *out, DataChunk *in) {
#ifdef DB7DEBUG

    const auto *out_column_ids = out->ColumnIds();
    const auto *in_column_ids = in->ColumnIds();
    for (u16 i = 0; in->GetColCount(); i++) {
      bool flag = false;
      for (u16 j = 0; out->GetColCount(); j++) {
        if (in_column_ids[i] == out_column_ids[j]) {
          flag = true;
          break;
        }
      }
      if (!flag) { throw CATALOG_EXCEPTION("Out chunk doesnt contain all of in chunks values"); }
    }

    return;
#endif
    DB7_UNREACHABLE();
  }
};

static_assert(sizeof(column_t) == 2);
static_assert(sizeof(DataChunk) == 8, "Sizes must match");

struct DataChunkDeleter {
  void operator()(DataChunk *chunk) const noexcept { DataChunk::Destroy(chunk); }
};
using DataChunkPtr = std::unique_ptr<DataChunk, DataChunkDeleter>;
} // namespace db7

namespace db7::storage {

class ChunkDiff {
public:
  /**
   * Byte-wise value equality of column `oid` in two chunks.
   * NULL == NULL, NULL != value. Varlen compares payloads, not slots,
   * because the heap offset inside a slot differs between chunks.
   * Conservative: values that are equal but have different bytes (e.g. -0.0 vs 0.0)
   * count as changed, which only costs an extra check, never a missed one.
   */
  static bool ValueEquals(DataChunk *a, DataChunk *b, column_t oid, const StorageColumnDesc &col) {
    byte *va = a->GetByOidNullCheck(oid);
    byte *vb = b->GetByOidNullCheck(oid);
    if (va == nullptr || vb == nullptr) { return va == vb; }

    if (!col.isVarlen) { return std::memcmp(va, vb, col.slot_size) == 0; }

    const auto *ea = VarlenEntry::Cast(va);
    const auto *eb = VarlenEntry::Cast(vb);
    const auto da = ea->Data(a->Bytes());
    const auto db = eb->Data(b->Bytes());
    return da.size() == db.size() && std::memcmp(da.data(), db.data(), da.size()) == 0;
  }

  /** Every column of `changes` whose value differs from `base`. Build it once per row if several
   * checks need it. */
  static shared::BitSet ChangedColumns(DataChunk *base, DataChunk *changes,
                                       const std::vector<StorageColumnDesc> &columns) {
    shared::BitSet out;
    const column_t *ids = changes->ColumnIds();
    for (u16 i = 0; i < changes->GetColCount(); i++) {
      if (!ValueEquals(base, changes, ids[i], columns[ids[i]])) { out.Add(ids[i]); }
    }
    return out;
  }
};

} // namespace db7::storage