#pragma once

#include "shared/monitoring/perf_util.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace db7::storage {
class BgWriter {
private:
  BufferPool &pool_;
  DiskScheduler &disk_scheduler_;
  std::chrono::milliseconds delay_{200};
  u64 max_pages_{100};

  std::thread thread_;
  std::mutex mu_;
  std::condition_variable cv_;
  bool stop_ = false;

private:
  frame_idx_t GetClock() const { return pool_.replacement_.GetCurrent(); }

  static bool TryStartFlush(Page &page) {
    flags_t state = page.State();
    while (Page::IsDirty(state) && Page::UsageCount(state) == 0 && Page::IsReclaimable(state)) {
      flags_t next = Page::SetWriteInProgress(Page::ClearDirty(state));
      if (page.CasState(state, next)) { return true; }
    }
    return false;
  }

  static void UndoFlush(Page *page) {
    flags_t state = page->State();
    flags_t next;
    do {
      next = Page::ClearWriteInProgress(Page::SetDirty(state));
    } while (!page->CasState(state, next));
    page->NotifyIO();
  }

  void Scan(u64 scan_count) {
    DB7_ASSERT(scan_count <= max_pages_, "Invalid scan count");
    const auto pages = pool_.GetPages();
    const size_t n = pages.size();
    const size_t start = GetClock();

    struct FlushItem {
      Page *page;

      PageIdentifier GetId() const { return page->GetId(); }
    };
    std::vector<FlushItem> items;
    items.reserve(scan_count);

    for (size_t i = 0; i < n && scan_count != 0; i++) { // at most one full lap
      Page &page = pages[(start + i) % n];
      if (!TryStartFlush(page)) { continue; }
      items.push_back({&page});
      scan_count--;
    }

    std::sort(items.begin(), items.end(), [](const FlushItem &a, const FlushItem &b) {
      if (a.GetId().tbl_id != b.GetId().tbl_id) return a.GetId().tbl_id < b.GetId().tbl_id;
      return a.GetId().pid < b.GetId().pid;
    });

    // Coalesce consecutive pages of the same table into runs of at most IO_MAX_COMBINE.
    Page *run[IO_MAX_COMBINE];
    for (size_t i = 0; i < items.size();) {
      const PageIdentifier first = items[i].GetId();
      u32 len = 0;
      while (i < items.size() && len < IO_MAX_COMBINE && items[i].GetId().tbl_id == first.tbl_id &&
             items[i].GetId().pid == first.pid + len) {
        run[len++] = items[i++].page;
      }
      bool ok;
      if (len == 1)
        ok = disk_scheduler_.Enqueue(IoTask(IoTask::WRITE, IoPriority::LOW, run[0], first));
      else
        ok = disk_scheduler_.Enqueue(IoTask(IoTask::WRITEV, IoPriority::LOW, run, len, first));

      if (!ok) [[unlikely]] { // disk scheduler out of memory leave the page for later
        for (u32 j = 0; j < len; j++) { UndoFlush(run[j]); }
      }
    }
  }

  void Run() {
    std::unique_lock lock(mu_);
    while (!stop_) {
      lock.unlock();
      Scan(max_pages_);
      lock.lock();
      cv_.wait_for(lock, delay_, [this] { return stop_; });
    }
  }

public:
  BgWriter(BufferPool &pool, DiskScheduler &disk_scheduler)
      : pool_(pool), disk_scheduler_(disk_scheduler) {}

  ~BgWriter() { Stop(); }

  void Start() {
    DB7_ASSERT(!thread_.joinable(), "BgWriter already started");
    stop_ = false;
    thread_ = std::thread(&BgWriter::Run, this);
    PerfUtil::SetName(thread_, "db7-bg-writer");
  }

  void Stop() {
    {
      std::lock_guard lock(mu_);
      stop_ = true;
    }
    cv_.notify_one();
    if (thread_.joinable()) { thread_.join(); }
  }
};
} // namespace db7::storage