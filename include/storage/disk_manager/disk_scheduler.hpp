#pragma once

#include "storage/disk_manager/disk_manager_async.hpp"
#include "storage/page.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

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
public:
  explicit DiskScheduler(DiskManagerAsync *io, u32 max_low_inflight = kMaxInflight / 4)
      : io_(io), max_low_inflight_(max_low_inflight) {
    DB7_ASSERT(max_low_inflight_ > 0 && max_low_inflight_ <= kMaxInflight, "bad low cap");
    for (u32 i = 0; i < kMaxInflight; i++) free_[i] = i;
    io_->SetCompletionCallback(
        [this](void *ud, ssize_t res) { OnComplete(static_cast<IoTask *>(ud), res); });
  }

  ~DiskScheduler() { Stop(); }

  void Start() {
    io_->InitWakeup();
    running_.store(true);
    worker_ = std::thread(&DiskScheduler::Run, this);
  }

  void Stop() {
    if (!running_.exchange(false)) return;
    io_->Wake(); // the worker may be blocked in WaitForCompletion
    if (worker_.joinable()) worker_.join();
  }

  // Creates this thread's producer tokens up front (e.g. at worker-thread startup) so the
  // first Enqueue doesn't hit the registry mutex. Optional: Enqueue registers lazily.
  void RegisterThread() { (void)Tokens(); }

  void Enqueue(IoTask task) {
    auto priority = u32(task.priority);
    queues_[priority].enqueue(*Tokens().tok[priority], std::move(task));
    std::atomic_thread_fence(std::memory_order_seq_cst); // pairs with the fence in Run()
    if (sleeping_.load(std::memory_order_relaxed) &&
        sleeping_.exchange(false, std::memory_order_relaxed)) {
      io_->Wake();
    }
  }

private:
  friend class BufferPool;
  static constexpr u32 kMaxInflight = IOURING_QUEUE_SIZE - 1; // one slot for the wakeup read

  // One explicit producer per (thread, priority queue).
  struct ThreadTokens {
    explicit ThreadTokens(moodycamel::ConcurrentQueue<IoTask> *qs) {
      for (u32 p = 0; p < kNumIoPriorities; p++) tok[p].emplace(qs[p]);
    }
    std::optional<moodycamel::ProducerToken> tok[kNumIoPriorities];
  };

  // Per-thread cache of "which scheduler did I last use, and where are my tokens".
  // Keyed by a never-reused instance id, not `this`, so a new scheduler allocated at a
  // dead one's address can't pick up dangling tokens.
  struct TlsCache {
    u64 owner_id = 0;
    ThreadTokens *tokens = nullptr;
  };
  static TlsCache &Tls() {
    static thread_local TlsCache cache;
    return cache;
  }

  ThreadTokens &Tokens() {
    TlsCache &c = Tls();
    if (c.owner_id == id_) [[likely]]
      return *c.tokens;
    return RegisterSlow();
  }

  ThreadTokens &RegisterSlow() {
    std::lock_guard lk(tokens_mu_);
    auto &slot = tokens_[std::this_thread::get_id()];
    if (!slot) slot = std::make_unique<ThreadTokens>(queues_);
    TlsCache &c = Tls();
    c.owner_id = id_;
    c.tokens = slot.get();
    return *slot;
  }

  static inline std::atomic<u64> next_id_{1};
  const u64 id_ = next_id_.fetch_add(1, std::memory_order_relaxed);

  DiskManagerAsync *io_;
  std::atomic<bool> running_{false};
  std::thread worker_;

  moodycamel::ConcurrentQueue<IoTask> queues_[kNumIoPriorities];

  // Declared after queues_ so tokens are destroyed before the queues they point into.
  std::mutex tokens_mu_;
  std::unordered_map<std::thread::id, std::unique_ptr<ThreadTokens>> tokens_;

  // touched only by the worker thread
  u32 inflight_ = 0;
  u32 inflight_by_prio_[kNumIoPriorities] = {};
  u32 max_low_inflight_;

  IoTask inflight_tasks_[kMaxInflight];
  u32 free_[kMaxInflight];
  u32 nfree_ = kMaxInflight;
  IoTask batch_[kMaxInflight]; // dequeue scratch space, kept off the stack

  alignas(64) std::atomic<bool> sleeping_{false};

  void Run() {
    while (running_.load()) {
      io_->ReapCompletions(kMaxInflight);
      if (SubmitPending()) continue; // made progress; there may be more queued

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
  bool SubmitPending() {
    u32 submitted = 0;
    for (u32 p = 0; p < kNumIoPriorities; p++) {
      u32 budget = kMaxInflight - inflight_;
      if (budget == 0) break;
      if (p == u32(IoPriority::LOW)) {
        budget = std::min(budget, max_low_inflight_ - inflight_by_prio_[p]);
        if (budget == 0) break;
      }
      size_t n = queues_[p].try_dequeue_bulk(batch_, budget);
      for (size_t i = 0; i < n; i++) SubmitOne(batch_[i]);
      submitted += u32(n);
    }
    if (submitted > 0) io_->Submit();
    return submitted > 0;
  }

  void SubmitOne(const IoTask &task) {
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

  void OnComplete(IoTask *t, ssize_t res) {
    ssize_t expected = ssize_t(t->count) * DB7_PAGE_SIZE;
    if (res != expected) {
      fprintf(stderr, "IO failed: tbl=%u pid=%u count=%u result=%zd (%s)\n", t->id.tbl_id,
              t->id.pid, t->count, res, res < 0 ? strerror(int(-res)) : "short");
      DB7_ASSERT(false, "IO failed");
    }
    // For WRITE/WRITEV, this is where to clear dirty if the page wasn't dirtied during the I/O.
    for (u32 j = 0; j < t->count; ++j) t->pages[j]->SignalIO();
    inflight_--;
    inflight_by_prio_[u32(t->priority)]--;
    FreeTask(t);
  }

  IoTask *AllocTask(const IoTask &t) {
    DB7_ASSERT(nfree_ > 0, "inflight pool exhausted");
    IoTask *slot = &inflight_tasks_[free_[--nfree_]];
    *slot = t;
    return slot;
  }

  void FreeTask(IoTask *t) { free_[nfree_++] = u32(t - inflight_tasks_); }
};

} // namespace db7::storage