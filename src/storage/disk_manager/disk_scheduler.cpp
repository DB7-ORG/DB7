#include "storage/disk_manager/disk_scheduler.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace db7::storage {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

DiskScheduler::DiskScheduler(DiskManagerAsync *io, u32 max_low_inflight)
    : io_(io), max_low_inflight_(max_low_inflight) {
  DB7_ASSERT(max_low_inflight_ > 0 && max_low_inflight_ <= kMaxInflight, "bad low cap");
  for (u32 i = 0; i < kMaxInflight; i++) free_[i] = i;
  io_->SetCompletionCallback(
      [this](void *ud, ssize_t res) { OnComplete(static_cast<IoTask *>(ud), res); });
}

DiskScheduler::~DiskScheduler() { Stop(); }

void DiskScheduler::Start() {
  io_->InitWakeup();
  running_.store(true);
  worker_ = std::thread(&DiskScheduler::Run, this);
}

void DiskScheduler::Stop() {
  if (!running_.exchange(false)) return;
  io_->Wake(); // the worker may be blocked in WaitForCompletion
  if (worker_.joinable()) worker_.join();
}

// ---------------------------------------------------------------------------
// Producer side
// ---------------------------------------------------------------------------

void DiskScheduler::RegisterThread() {
  for (auto &q : queues_) q.RegisterThread();
}

void DiskScheduler::Enqueue(IoTask task) {
  auto priority = u32(task.priority);
  queues_[priority].Enqueue(std::move(task));
  std::atomic_thread_fence(std::memory_order_seq_cst); // pairs with the fence in Run()
  if (sleeping_.load(std::memory_order_relaxed) &&
      sleeping_.exchange(false, std::memory_order_relaxed)) {
    io_->Wake();
  }
}

// ---------------------------------------------------------------------------
// Worker side (touched only by the worker thread)
// ---------------------------------------------------------------------------

void DiskScheduler::Run() {
  while (running_.load()) {
    io_->ReapCompletions(kMaxInflight);
    if (SubmitPending()) { continue; }

    // Nothing could be submitted: queues are empty or the budget is full.
    sleeping_.store(true, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    // Re-check by trying again; this closes the race with Enqueue.
    if (!SubmitPending() && running_.load()) io_->WaitForCompletion();
    sleeping_.store(false, std::memory_order_relaxed);
  }

  // Shutdown: finish everything already queued so no page waiter hangs.
  for (;;) {
    io_->ReapCompletions(kMaxInflight);
    bool progressed = SubmitPending();
    if (!progressed && inflight_ == 0) break;
    if (!progressed) io_->WaitForCompletion();
  }
}

// Strict priority: CRITICAL, then HIGH, then LOW (LOW also has its own cap).
bool DiskScheduler::SubmitPending() {
  u32 submitted = 0;
  for (u32 p = 0; p < kNumIoPriorities; p++) {
    u32 budget = kMaxInflight - inflight_;
    if (budget == 0) break;
    if (p == u32(IoPriority::LOW)) {
      budget = std::min(budget, max_low_inflight_ - inflight_by_prio_[p]);
      if (budget == 0) break;
    }
    size_t n = queues_[p].TryDequeueBulk(batch_, budget);
    for (size_t i = 0; i < n; i++) SubmitOne(batch_[i]);
    submitted += u32(n);
  }
  if (submitted > 0) io_->Submit();
  return submitted > 0;
}

void DiskScheduler::SubmitOne(const IoTask &task) {
  IoTask *t = AllocTask(task);
  off_t off = off_t(t->id.pid - 1) * DB7_PAGE_SIZE;
  bool ok = false;
  switch (t->op) {
  case IoTask::READ:
    ok = io_->SubmitRead(t->id.tbl_id, t->pages[0]->GetData(), DB7_PAGE_SIZE, off, t);
    break;
  case IoTask::WRITE:
    ok = io_->SubmitWrite(t->id.tbl_id, t->pages[0]->GetData(), DB7_PAGE_SIZE, off, t);
    break;
  case IoTask::READV: ok = io_->SubmitReadv(t->id.tbl_id, t->pages, t->count, off, t); break;
  case IoTask::WRITEV: ok = io_->SubmitWritev(t->id.tbl_id, t->pages, t->count, off, t); break;
  default: DB7_UNREACHABLE();
  }
  // At most kMaxInflight task SQEs plus one wakeup SQE are queued between submits,
  // so the SQ cannot be full here.
  DB7_ASSERT(ok, "SQ full despite inflight budget");
  inflight_++;
  inflight_by_prio_[u32(t->priority)]++;
}

void DiskScheduler::OnComplete(IoTask *t, ssize_t res) {
  ssize_t expected = ssize_t(t->count) * DB7_PAGE_SIZE;
  if (res != expected) {
    fprintf(stderr, "IO failed: tbl=%u pid=%u count=%u result=%zd (%s)\n", t->id.tbl_id, t->id.pid,
            t->count, res, res < 0 ? strerror(int(-res)) : "short");
    DB7_ASSERT(false, "IO failed");
  }
  // For WRITE/WRITEV, this is where to clear dirty if the page wasn't dirtied during the I/O.
  for (u32 j = 0; j < t->count; ++j) t->pages[j]->SignalIO();
  inflight_--;
  inflight_by_prio_[u32(t->priority)]--;
  FreeTask(t);
}

IoTask *DiskScheduler::AllocTask(const IoTask &t) {
  DB7_ASSERT(nfree_ > 0, "inflight pool exhausted");
  IoTask *slot = &inflight_tasks_[free_[--nfree_]];
  *slot = t;
  return slot;
}

void DiskScheduler::FreeTask(IoTask *t) { free_[nfree_++] = u32(t - inflight_tasks_); }

} // namespace db7::storage