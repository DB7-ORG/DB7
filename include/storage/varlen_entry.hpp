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
  const char data_[12];
  const u16 offset_;

private:
  friend class Pax;
  u16 Offset() const { return offset_; }

public:
  VarlenEntry() = delete;

  static VarlenEntry *Cast(byte *slot) { return reinterpret_cast<VarlenEntry *>(slot); }

  u16 Size() const { return size_; }
  bool IsInlined() const { return size_ <= SizeOf(type_id::VARCHAR); }
};

static_assert(sizeof(VarlenEntry) == SizeOf(type_id::VARCHAR), "Entry size mismatch");

} // namespace db7::storage