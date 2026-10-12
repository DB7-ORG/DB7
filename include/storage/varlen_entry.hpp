#pragma once

#include "common.hpp"
#include "shared/types/type_defs.hpp"

#include <cstring>
#include <span>

// TODO fix includes

namespace db7::storage {

class VarlenEntryView {
private:
  u16 size_;
  std::span<byte> prefix_;
  std::span<byte> rest_;

public:
  VarlenEntryView(u16 size, std::span<byte> prefix, std::span<byte> rest)
      : size_(size), prefix_(prefix), rest_(rest) {}

  std::span<byte> Prefix();
  std::span<byte> Rest();
};

class VarlenEntry {
private:
  u16 size_;
  const byte data_[12];
  const u16 offset_;

private:
  friend class Pax;
  u16 Offset() const { return offset_; }

public:
  VarlenEntry() = delete;

  static VarlenEntry *Cast(byte *slot) { return reinterpret_cast<VarlenEntry *>(slot); }

  u16 Size() const { return size_; }
  bool IsInlined() const { return size_ <= SizeOf(type_id::VARCHAR); }

  VarlenEntryView View(byte *page) {
    return VarlenEntryView(size_, {&data_, 14}, {page + offset_, 17});
  }
};

static_assert(sizeof(VarlenEntry) == SizeOf(type_id::VARCHAR), "Entry size mismatch");

} // namespace db7::storage