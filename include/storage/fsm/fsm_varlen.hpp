#pragma once

#include "storage/disk_manager/disk_manager_async.hpp"
#include "storage/storage_common.hpp"

namespace db7::storage {
class FreeSpaceManagerVarlen {
private:
  mutable std::mutex mu_;
  u32 next_page_id = 1;
  u32 free_space = DB7_PAGE_SIZE / 2; // TODO should be max index
  storage::DiskManagerAsync &disk_;
  table_id tid_;

private:
  static constexpr u32 kGrowPages = 64;
  void EnsureAllocated(u32 page_id) {
    if (page_id >= disk_.PageCount(tid_)) disk_.ExtendFile(tid_, kGrowPages);
  }

public:
  FreeSpaceManagerVarlen(storage::DiskManagerAsync &disk, table_id tid) : disk_(disk), tid_(tid) {}

  u32 Get(u32 space) {
    std::lock_guard lock(mu_);
    if (free_space < space) {
      next_page_id++;
      free_space = DB7_PAGE_SIZE / 2;
    }
    EnsureAllocated(next_page_id);
    free_space -= space;
    return next_page_id;
  }

  u32 GetPageCount() const {
    std::lock_guard lock(mu_);
    return next_page_id + 1;
  }
};
} // namespace db7::storage