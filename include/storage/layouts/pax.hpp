#pragma once

#include "common.hpp"
#include "storage/data_chunk.hpp"
#include "storage/page.hpp"

namespace db7::storage {
class PaxPageHeader {
public:
  static idx_t Count(storage::Page *page) {
    byte *data = page->GetData();
    return *reinterpret_cast<idx_t *>(data);
  }
};

class Pax {
public:
  void Insert(DataChunk &chunk) {}
};
} // namespace db7::storage