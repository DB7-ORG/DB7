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
inline constexpr u32 IO_MAX_COMBINE = 16;

struct IoTask {
  enum Op { READ, WRITE, READV };
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

  void Run() {
    io_->SetCompletionCallback([](void *user_data, ssize_t result) {
      (void)result;
      auto *page = static_cast<Page *>(user_data);
      if (result != DB7_PAGE_SIZE) {
        fprintf(stderr, "IO failed: page tbl=%u pid=%u result=%zd (%s)\n", page->GetId().tbl_id,
                page->GetId().pid, result, result < 0 ? strerror((int)-result) : "short");
      }
      DB7_ASSERT(result == DB7_PAGE_SIZE, "Short read");
      DB7_ASSERT(result == DB7_PAGE_SIZE, "Short read");
      page->SignalIO();
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
      bool ok;
      switch (tasks[i].op) {
      case IoTask::READ:
        ok = io_->SubmitRead(tasks[i].id.tbl_id, tasks[i].pages[0]->GetData(), DB7_PAGE_SIZE,
                             (tasks[i].id.pid - 1) * DB7_PAGE_SIZE, (void *)tasks[i].pages[0]);
        break;
      case IoTask::WRITE:
        ok = io_->SubmitWrite(tasks[i].id.tbl_id, tasks[i].pages[0]->GetData(), DB7_PAGE_SIZE,
                              (tasks[i].id.pid - 1) * DB7_PAGE_SIZE, (void *)tasks[i].pages[0]);
        break;
      case IoTask::READV: ok = io_->SubmitReadv(); break;
      default: throw;
      }

      if (ok) {
        inflight_++;
        submitted_any = true;
      } else {
        // Ring full — re-enqueue this task and all remaining ones
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
        queue_(IOURING_QUEUE_SIZE) {}

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