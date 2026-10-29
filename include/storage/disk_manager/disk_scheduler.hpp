#pragma once

#include "storage/disk_manager/disk_manager_async.hpp"
#include "storage/page.hpp"

#include <atomic>
#include <thread>

namespace db7::storage {
enum class IoPriority : u8 {
  CRITICAL, // WAL flush, transaction commit
  HIGH,     // page reads for active queries
  LOW       // background tasks, prefetch
};

struct IoTask {
  enum Op { READ, WRITE, READV, WRITEV };
  Op op;
  IoPriority priority;
  PageIdentifier id; // first page
  u8 count = 0;
  Page *pages[IO_MAX_COMBINE];

  IoTask() {}

  IoTask(Op op, IoPriority priority, Page *page, PageIdentifier id)
      : op(op), priority(priority), id(id), count(1) {
    pages[0] = page;
  }

  IoTask(Op op, IoPriority priority, Page *const *pages_in, u32 n, PageIdentifier first)
      : op(op), priority(priority), id(first), count(static_cast<u8>(n)) {
    DB7_ASSERT(n >= 1 && n <= IO_MAX_COMBINE, "invalid io size");
    std::copy_n(pages_in, n, pages);
  }

  // higher priority = should come first
  bool operator<(const IoTask &other) const {
    return priority > other.priority; // reversed for min-heap
  }
};

class DiskScheduler {
private:
  friend class BufferPool;
  DiskManagerAsync *io_;
  u32 inflight_;
  u32 max_inflight_;
  std::atomic<bool> running_;

  moodycamel::ConcurrentQueue<IoTask> queue_;
  std::thread worker_;

private:
  IoTask inflight_tasks_[IOURING_QUEUE_SIZE];
  u32 free_[IOURING_QUEUE_SIZE];
  u32 nfree_ = IOURING_QUEUE_SIZE;

  IoTask *AllocTask(const IoTask &t) {
    DB7_ASSERT(nfree_ > 0, "inflight pool exhausted");
    IoTask *slot = &inflight_tasks_[free_[--nfree_]];
    *slot = t;
    return slot;
  }
  void FreeTask(IoTask *t) { free_[nfree_++] = u32(t - inflight_tasks_); }

  void Run() {
    io_->SetCompletionCallback([this](void *user_data, ssize_t result) {
      auto *t = static_cast<IoTask *>(user_data);
      ssize_t expected = ssize_t(t->count) * DB7_PAGE_SIZE;
      if (result != expected) {
        fprintf(stderr, "IO failed: tbl=%u pid=%u count=%u result=%zd (%s)\n", t->id.tbl_id,
                t->id.pid, t->count, result, result < 0 ? strerror(int(-result)) : "short");
        DB7_ASSERT(false, "IO failed");
      }
      for (u32 j = 0; j < t->count; ++j) t->pages[j]->SignalIO();
      FreeTask(t);
    });

    while (running_) {
      // 1. drain completions if any are ready
      DrainCompletions();

      // 2. submit as many queued tasks as inflight budget allows
      SubmitPending();

      // 3. if nothing to do, sleep
      if (queue_.size_approx() == 0 && inflight_ == 0) {
        // Nothing to do — back off briefly
        std::this_thread::sleep_for(std::chrono::microseconds(50));
      }
    }

    // drain remaining on shutdown
    while (inflight_ > 0) DrainCompletions();
  }

  void SubmitPending() {
    bool submitted_any = false;

    // Bulk dequeue up to available inflight slots
    u32 slots = std::min((u32)IOURING_QUEUE_SIZE, max_inflight_ - inflight_);
    if (slots == 0) return;
    IoTask tasks[IOURING_QUEUE_SIZE]; // or use max_inflight_ with a vector if
                                      // it varies
    size_t count = queue_.try_dequeue_bulk(tasks, slots);

    for (size_t i = 0; i < count; ++i) {
      IoTask *t = AllocTask(tasks[i]);
      off_t off = off_t(t->id.pid - 1) * DB7_PAGE_SIZE;
      bool ok;
      switch (t->op) {
      case IoTask::READ: {
        ok = io_->SubmitRead(t->id.tbl_id, t->pages[0]->GetData(), DB7_PAGE_SIZE, off, t);
        break;
      }
      case IoTask::WRITE: {
        ok = io_->SubmitWrite(t->id.tbl_id, t->pages[0]->GetData(), DB7_PAGE_SIZE, off, t);
        break;
      }
      case IoTask::READV: {
        ok = io_->SubmitReadv(t->id.tbl_id, t->pages, t->count, off, t);
        break;
      }
      case IoTask::WRITEV: {
        ok = io_->SubmitWritev(t->id.tbl_id, t->pages, t->count, off, t);
        break;
      }
      default: DB7_UNREACHABLE();
      }

      if (ok) {
        inflight_++;
        submitted_any = true;
      } else {
        // Ring full — re-enqueue this task and all remaining ones
        FreeTask(t);
        queue_.enqueue_bulk(tasks + i,
                            count - i); // should probl use try_enqueue
        break;
      }
    }

    if (submitted_any) io_->Submit();
  }

  void DrainCompletions() {
    if (inflight_ > 0) { inflight_ -= io_->ReapCompletions(max_inflight_); }
  }

public:
  DiskScheduler(DiskManagerAsync *io)
      : io_(io), inflight_(0), max_inflight_(IOURING_QUEUE_SIZE), running_(false),
        queue_(IOURING_QUEUE_SIZE) {
    for (u32 i = 0; i < IOURING_QUEUE_SIZE; i++) free_[i] = i;
  }

  ~DiskScheduler() { Stop(); }

  void Start() {
    running_ = true;
    worker_ = std::thread(&DiskScheduler::Run, this);
  }

  void Stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
  }

  void Enqueue(IoTask task) { queue_.enqueue(task); }
};
} // namespace db7::storage