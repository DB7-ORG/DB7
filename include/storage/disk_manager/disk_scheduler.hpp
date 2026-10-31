#pragma once

#include "storage/disk_manager/disk_manager_async.hpp"
#include "storage/disk_manager/token_queue.hpp"
#include "storage/page.hpp"

#include <atomic>
#include <thread>

namespace db7::storage {

enum class IoPriority : u8 {
  CRITICAL = 0, // WAL flush, transaction commit
  HIGH = 1,     // page reads for active queries
  LOW = 2       // background writes, prefetch
};
inline constexpr u32 kNumIoPriorities = 3;

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
};

class DiskScheduler {

private:
  friend class BufferPool;
  static constexpr u32 kMaxInflight = IOURING_QUEUE_SIZE - 1; // one slot for the wakeup read
  DiskManagerAsync *io_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  shared::TokenQueue<IoTask> queues_[kNumIoPriorities];

  // touched only by the worker thread
  u32 inflight_ = 0;
  u32 inflight_by_prio_[kNumIoPriorities] = {};
  u32 max_low_inflight_;
  IoTask inflight_tasks_[kMaxInflight];
  u32 free_[kMaxInflight];
  u32 nfree_ = kMaxInflight;
  IoTask batch_[kMaxInflight];
  alignas(64) std::atomic<bool> sleeping_{false};

private:
  void Run();
  bool SubmitPending();
  void SubmitOne(const IoTask &task);
  void OnComplete(IoTask *t, ssize_t res);
  IoTask *AllocTask(const IoTask &t);
  void FreeTask(IoTask *t);

public:
  explicit DiskScheduler(DiskManagerAsync *io, u32 max_low_inflight = kMaxInflight / 4);
  ~DiskScheduler();

  void Start();
  void Stop();

  // Optional warm-up: creates this thread's producer tokens in all priority queues.
  // Without it, each queue creates the token on this thread's first Enqueue to it.
  void RegisterThread();

  void Enqueue(IoTask task);
};

} // namespace db7::storage