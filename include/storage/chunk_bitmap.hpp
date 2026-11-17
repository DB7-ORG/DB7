#pragma once

#include "common.hpp"

namespace db7 {
class ChunkBitmap {
private:
  byte *block_;

public:
  ChunkBitmap() noexcept : block_(nullptr) {}

  explicit ChunkBitmap(void *block) noexcept : block_(reinterpret_cast<byte *>(block)) {}

  bool IsNull(u16 idx) const { return ((block_[idx / 8] >> (idx % 8)) & 1); }

  void SetNull(u16 idx, bool isNull) {
    const byte mask = byte(1u << (idx % 8));
    if (isNull)
      block_[idx / 8] |= mask;
    else
      block_[idx / 8] &= byte(~mask);
  }

  byte *Data() const noexcept { return block_; }

  explicit operator bool() const noexcept { return block_ != nullptr; }
};

struct ChunkColumn {
  // column id
  column_t oid;
  // size of fixed part (pow of 2).
  // NOTE: for warlen is 16 bytes not size of payload
  u16 size;

  ChunkColumn() : oid(0), size(0) {}
  ChunkColumn(column_t o, u16 s) : oid(o), size(s) {}
};
} // namespace db7