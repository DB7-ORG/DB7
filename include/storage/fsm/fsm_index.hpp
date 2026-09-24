#pragma once

#include "storage/storage_common.hpp"

#include <mutex>
#include <unordered_map>

namespace db7::storage {
class FreeSpaceManagerIndex {
private:
  inline static std::mutex mtx_;
  inline static std::unordered_map<table_id, u32> page_ids_;

public:
  static u32 Get(table_id tbl_id) {
    std::lock_guard lock(mtx_);
    auto [it, inserted] = page_ids_.try_emplace(tbl_id, 1);
    return it->second++;
  }

  static void Reset() {
    std::lock_guard lock(mtx_);
    page_ids_.clear();
  }
};
} // namespace db7::storage