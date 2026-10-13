#pragma once

#include "common.hpp"
#include "shared/types/type_defs.hpp"

#include <cstring>
#include <span>

// TODO fix includes

namespace db7::storage {

class VarlenEntry {
public:
  static constexpr u16 slot_size = SizeOf(type_id::VARCHAR);
  static constexpr u16 inline_bytes = slot_size - sizeof(u16);

  static constexpr u16 HeapSizeFor(u16 size) {
    return size <= slot_size ? 0 : u16(size - inline_bytes);
  }

private:
  static constexpr auto entry_data_size = slot_size - sizeof(u16) - sizeof(u16); // 12

private:
  u16 size_;
  [[maybe_unused]] byte data_[entry_data_size];
  u16 offset_;

public:
  VarlenEntry() = delete;

  static VarlenEntry *Cast(byte *slot) {
    DB7_ASSERT(reinterpret_cast<uintptr_t>(slot) % alignof(VarlenEntry) == 0, "Not aligned cast");
    return reinterpret_cast<VarlenEntry *>(slot);
  }
  static const VarlenEntry *Cast(const byte *slot) {
    DB7_ASSERT(reinterpret_cast<uintptr_t>(slot) % alignof(VarlenEntry) == 0, "Not aligned cast");
    return reinterpret_cast<const VarlenEntry *>(slot);
  }

  u16 Size() const { return size_; }
  bool IsInlined() const { return size_ <= slot_size; }
  u16 HeapSize() const { return HeapSizeFor(size_); }
  u16 Offset() const {
    DB7_ASSERT(!IsInlined(), "offset read on inlined entry");
    return offset_;
  }
  void SetOffset(u16 offset) {
    DB7_ASSERT(!IsInlined(), "offset write would clobber inlined payload");
    offset_ = offset;
  }

  std::span<const byte> Data(const byte *base) const { return IsInlined() ? Prefix() : Heap(base); }

  std::span<const byte> Slot() const { return {reinterpret_cast<const byte *>(this), slot_size}; }

  std::span<const byte> Heap(const byte *base) const { return {base + Offset(), HeapSize()}; }

  std::span<const byte> Prefix() const {
    return {reinterpret_cast<const byte *>(&data_), slot_size - sizeof(u16)};
  }
};

static_assert(sizeof(VarlenEntry) == SizeOf(type_id::VARCHAR), "Entry size mismatch");

} // namespace db7::storage