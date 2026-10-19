#pragma once

#include "storage/storage_common.hpp"

namespace db7::storage {
class FreeSpaceManagerVarlen {
private:
  inline static u32 next_page_id = 1;
  inline static u32 free_space = DB7_PAGE_SIZE / 2; // TODO should be max index

public:
  static u32 Get(u32 space) {
    if (free_space < space) {
      next_page_id++;
      free_space = DB7_PAGE_SIZE / 2;
    }
    free_space -= space;
    return next_page_id;
  }
};
} // namespace db7::storage