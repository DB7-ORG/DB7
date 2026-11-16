#pragma once

#include "common.hpp"
#include "shared/types/type_defs.hpp"

#include <cstdint>
#include <cstring>
#include <span>

// TODO fix includes

namespace db7::storage {

class VarlenEntry {
public:
  static constexpr u16 slot_size = SizeOf(type_id::VARCHAR);
  static constexpr u16 inline_bytes = slot_size - sizeof(u16);

  static constexpr u16 HeapSizeFor(u16 size) { return size <= inline_bytes ? 0 : u16(size); }

private:
  static constexpr auto entry_data_size = slot_size - sizeof(u16) - sizeof(void *); // 6

private:
  u16 size_;
  [[maybe_unused]] byte data_[entry_data_size];
  u64 ref_;

private:
  bool IsSwizzled() const { return ref_ > UINT16_MAX; }

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
  bool IsInlined() const { return size_ <= inline_bytes; }
  u16 HeapSize() const { return HeapSizeFor(size_); }
  u16 Offset() const {
    DB7_ASSERT(!IsInlined(), "offset read on inlined entry");
    return static_cast<u16>(ref_);
  }
  void SetOffset(u16 offset) {
    DB7_ASSERT(!IsInlined(), "offset write would clobber inlined payload");
    ref_ = offset;
  }

  void Swizzle(const byte *base) {
    DB7_ASSERT(!IsInlined(), "pointer write would clobber inlined payload");
    ref_ = reinterpret_cast<uintptr_t>(base + Offset());
  }

  const byte *Ptr() const {
    DB7_ASSERT(!IsInlined() && IsSwizzled(), "not a pointer entry");
    return reinterpret_cast<const byte *>(static_cast<uintptr_t>(ref_));
  }

  std::span<const byte> Data(const byte *base) const { return IsInlined() ? Prefix() : Heap(base); }

  // std::span<const byte> Slot() const { return {reinterpret_cast<const byte *>(this), slot_size};
  // }

  std::span<const byte> Heap(const byte *base) const { return {base + Offset(), HeapSize()}; }

  std::span<const byte> Prefix() const {
    const byte *p = reinterpret_cast<const byte *>(this) + sizeof(u16);
    return {p, IsInlined() ? size_ : u16(entry_data_size)};
  }
};

static_assert(sizeof(void *) == 8);
static_assert(sizeof(VarlenEntry) == 16 && alignof(VarlenEntry) == 8);
static_assert(sizeof(VarlenEntry) == SizeOf(type_id::VARCHAR), "Entry size mismatch");

} // namespace db7::storage