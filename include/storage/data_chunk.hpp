#pragma once

#include "common.hpp"

#include <span>

namespace db7 {

class DataChunk {
public:
  byte *Get(idx_t oid) { return nullptr; };

  u32 GetSize() { return 0; };

  std::span<byte> GetHeaderPtr() { return {}; }
};
} // namespace db7