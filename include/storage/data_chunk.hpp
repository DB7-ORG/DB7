#pragma once

#include "common.hpp"

#include <span>

namespace db7 {
/** TODO add padd
 * [total size ib bytes] (2 bytes)
 * [column count]        (2 bytes)
 * [column ids]          (count* sizeof(col_id))
 * [offsets]             (count* 2 bytes)
 * [null bitmap]         (coulumn count/8 + 1 bytes)
 * [data]                (varlen)
 */
class DataChunk {
  byte *base_;

public:
  byte *Get(idx_t oid) { return nullptr; };

  u32 GetSize() { return 0; };

  std::span<byte> GetHeaderPtr() { return {}; }
};
} // namespace db7