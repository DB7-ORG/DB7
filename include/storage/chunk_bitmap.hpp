#pragma once

#include "common.hpp"

namespace db7 {
class ChunkBitmap {
private:
  byte *block_;

public:
  template <typename T>
  explicit ChunkBitmap(T *block) noexcept : block_(block) {}

  bool IsNull(u16 idx) const { return ((block_[idx / 8] >> (idx % 8)) & 1); }

  void SetNull(u16 idx, bool isNull) {
    const byte mask = byte(1u << (idx % 8));
    if (isNull)
      block_[idx / 8] |= mask;
    else
      block_[idx / 8] &= byte(~mask);
  }

  byte *Data() const noexcept { return block_; }
};

struct ChunkColumn {
  // column id
  column_t oid;
  // size of fixed part (pow of 2).
  // NOTE: for warlen is 16 bytes not size of payload
  u16 size;

  ChunkColumn(column_t o, u16 s) : oid(o), size(s) {}
};
} // namespace db7