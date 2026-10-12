#pragma once

#include "common.hpp"
#include "shared/types/type_defs.hpp"

#include <cstring>
#include <span>

// TODO fix includes

namespace db7::storage {

class VarlenEntry {
private:
  u16 size_;
  const byte data_[12];
  u16 offset_;

public:
  static constexpr u16 slot_size = SizeOf(type_id::VARCHAR);
  static constexpr u16 inline_bytes = slot_size - sizeof(u16);

  VarlenEntry() = delete;

  static VarlenEntry *Cast(byte *slot) { return reinterpret_cast<VarlenEntry *>(slot); }
  static const VarlenEntry *Cast(const byte *slot) {
    return reinterpret_cast<const VarlenEntry *>(slot);
  }

  u16 Size() const { return size_; }
  bool IsInlined() const { return size_ <= slot_size; }
  u16 HeapSize() const { return IsInlined() ? 0 : u16(size_ - inline_bytes); }
  u16 Offset() const {
    DB7_ASSERT(!IsInlined(), "Trash data its inlined");
    return offset_;
  }
  u16 SetOffset(u16 offset) { offset_ = offset; }

  std::span<const byte> Slot() const { return {reinterpret_cast<const byte *>(this), slot_size}; }

  std::span<const byte> Heap(const byte *base) const { return {base + offset_, HeapSize()}; }
};

static_assert(sizeof(VarlenEntry) == SizeOf(type_id::VARCHAR), "Entry size mismatch");

} // namespace db7::storage