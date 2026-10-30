#pragma once

#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_manager_async.hpp"

namespace db7::storage {
class BgWriter {
private:
  BufferPool &pool_;
  DiskManagerAsync &disk_;
  u64 delay_;
  u64 max_pages_;

private:
  frame_idx_t GetClock() const { return pool_.replacement_.GetCurrent(); }

  struct FlushItem {
    PageIdentifier id;
    Page *page; // lets the flusher go straight to the frame, no page-table lookup
  };

  void Scan(u64 scan_count) {
    DB7_ASSERT(scan_count <= max_pages_, "Invalid scan count");
    const auto pages = pool_.GetPages();
    const frame_idx_t n = pages.size();
    const frame_idx_t start = GetClock();

    std::vector<FlushItem> items;
    items.reserve(scan_count);

    for (frame_idx_t i = 0; i < n && scan_count != 0; i++) { // at most one full lap
      const u64 idx = (start + i) % n;
      Page &page = pages[idx];
      if (page.IsPinned() || page.UsageCount() > 0 || !page.IsDirty()) continue;
      if (page.TryRDataLock()) {
        items.push_back({page.GetId(), &page});
        scan_count--;
      }
    }

    std::sort(items.begin(), items.end(), [](const FlushItem &a, const FlushItem &b) {
      if (a.id.tbl_id != b.id.tbl_id) return a.id.tbl_id < b.id.tbl_id;
      return a.id.pid < b.id.pid;
    });

    for (frame_idx_t i = 0; i < items.size(); i++) {
      FlushItem item = items[i];
      PageIdentifier id = item.id;
      off_t off = off_t(id.pid - 1) * DB7_PAGE_SIZE;
      i++;
      while (i < items.size() && id.tbl_id == items[i].id.tbl_id && id.pid + i == items[i].id.pid) {
        i++;
      }

      if (i > 0) {
        auto ok = disk_.SubmitWritev(id.tbl_id, &item.page, i, off, nullptr);
        if (!ok) {}
      }
    }
  }

public:
  BgWriter(BufferPool &pool, DiskManagerAsync &disk) : pool_(pool), disk_(disk) {}

  void Start() {}
  void Stop() {}
};
} // namespace db7::storage