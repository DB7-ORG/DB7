#pragma once

#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"

namespace db7::storage {
class BgWriter {
private:
  BufferPool &pool_;
  DiskScheduler &disk_scheduler_;
  u64 delay_;
  u64 max_pages_;

private:
  frame_idx_t GetClock() const { return pool_.replacement_.GetCurrent(); }

  void Scan(u64 scan_count) {
    DB7_ASSERT(scan_count <= max_pages_, "Invalid scan count");
    const auto pages = pool_.GetPages();
    const size_t n = pages.size();
    const size_t start = GetClock();

    struct FlushItem {
      PageIdentifier id;
      Page *page;
    };
    std::vector<FlushItem> items;
    items.reserve(scan_count);

    for (size_t i = 0; i < n && scan_count != 0; i++) { // at most one full lap
      Page &page = pages[(start + i) % n];
      if (!page.TryWLock()) continue;
      if (page.IsPinned() || page.UsageCount() > 0 || !page.IsDirty() || page.IsIOInProgress() ||
          page.IsWriteInProgress() || !page.TryRDataLock()) {
        page.WUnlock();
        continue;
      }
      page.Pin();
      page.SetWriteInProgress();
      items.push_back({page.GetId(), &page});
      page.WUnlock();
      scan_count--;
    }

    std::sort(items.begin(), items.end(), [](const FlushItem &a, const FlushItem &b) {
      if (a.id.tbl_id != b.id.tbl_id) return a.id.tbl_id < b.id.tbl_id;
      return a.id.pid < b.id.pid;
    });

    // Coalesce consecutive pages of the same table into runs of at most IO_MAX_COMBINE.
    Page *run[IO_MAX_COMBINE];
    for (size_t i = 0; i < items.size();) {
      const PageIdentifier first = items[i].id;
      u32 len = 0;
      while (i < items.size() && len < IO_MAX_COMBINE && items[i].id.tbl_id == first.tbl_id &&
             items[i].id.pid == first.pid + len) {
        run[len++] = items[i++].page;
      }
      bool ok;
      if (len == 1)
        ok = disk_scheduler_.Enqueue(IoTask(IoTask::WRITE, IoPriority::LOW, run[0], first));
      else
        ok = disk_scheduler_.Enqueue(IoTask(IoTask::WRITEV, IoPriority::LOW, run, len, first));

      if (!ok) {
        // TODO do something, make sure to not leak anything
      }
    }

    for (auto item : items) {
      item.page->WaitWrite();
      item.page->ClearDirty();
      item.page->RDataUnlock();
      item.page->Unpin();
    }
  }

public:
  BgWriter(BufferPool &pool, DiskScheduler &disk_scheduler)
      : pool_(pool), disk_scheduler_(disk_scheduler) {}

  void Start() {}
  void Stop() {}
};
} // namespace db7::storage