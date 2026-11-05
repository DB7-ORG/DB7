#include <gtest/gtest.h>

#include "concurrency.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <thread>
#include <vector>

using namespace db7;
using namespace db7::storage;
using namespace db7::test;

namespace {

// =========================================================================
// 1. TEST DATA
//
//    Every page holds one number, stored at both ends of the page. If the
//    two copies disagree, the frame was reused or changed mid-transfer.
// =========================================================================

constexpr auto kKind = PageKind::Heap;
constexpr u32 kHead = 0;
constexpr u32 kTail = DB7_PAGE_SIZE - sizeof(u64);

/// Raise this when running under a sanitizer. A hang is a real failure here,
/// and gtest has no timeout of its own.
constexpr auto kTestTimeout = std::chrono::seconds(180);

u64 StampFor(PageIdentifier id) { return (static_cast<u64>(id.tbl_id) << 32) | id.pid; }

/// Caller holds the page's write lock.
void Store(Page *page, u64 value) {
  page->WriteOffset<u64>(kHead, value);
  page->WriteOffset<u64>(kTail, value);
}

/// Caller holds a data lock on the page. Returns nullopt if the page is torn.
std::optional<u64> Load(Page *page) {
  u64 head = 0;
  u64 tail = 0;
  std::memcpy(&head, page->GetOffset(kHead), sizeof(u64));
  std::memcpy(&tail, page->GetOffset(kTail), sizeof(u64));
  if (head != tail) { return std::nullopt; }
  return head;
}

/// Page keeps its pin count and flags in one word. These read a snapshot of it.
u32 PinsOf(const Page *page) { return Page::PinCount(page->State()); }
bool IsPinned(const Page *page) { return Page::IsPinned(page->State()); }
bool IsDirty(const Page *page) { return Page::IsDirty(page->State()); }

/// Aborts the run if a test outlives kTestTimeout, so a hang can't block forever.
class HangGuard {
private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;
  std::thread thread_;

public:
  HangGuard()
      : thread_([this] {
          std::unique_lock lock(mutex_);
          if (cv_.wait_for(lock, kTestTimeout, [this] { return stop_; })) { return; }
          const auto *test = ::testing::UnitTest::GetInstance()->current_test_info();
          std::fprintf(stderr, "HANG: %s.%s passed kTestTimeout\n", test->test_suite_name(),
                       test->name());
          std::abort();
        }) {}

  ~HangGuard() {
    {
      std::lock_guard lock(mutex_);
      stop_ = true;
    }
    cv_.notify_all();
    thread_.join();
  }
};

/// Prints progress once a second, and dumps the frames if nothing moved.
/// Put it in its own scope around RunParallel, so it stops before reporting.
class Progress {
private:
  std::atomic<u64> done_{0};
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;
  std::thread thread_;

public:
  Progress(u64 total, Page *pages, frame_idx_t frames)
      : thread_([this, total, pages, frames] {
          u64 last = 0;
          std::unique_lock lock(mutex_);
          while (!cv_.wait_for(lock, std::chrono::seconds(1), [this] { return stop_; })) {
            const u64 now = done_;
            std::fprintf(stderr, "%llu / %llu (+%llu)\n", (unsigned long long)now,
                         (unsigned long long)total, (unsigned long long)(now - last));
            if (now == last) {
              for (frame_idx_t f = 0; f < std::min<frame_idx_t>(frames, 32); ++f) {
                std::fprintf(stderr, "  frame %u: pid=%u pins=%u io=%d write=%d dirty=%d\n",
                             (unsigned)f, (unsigned)pages[f].GetPageId(), PinsOf(&pages[f]),
                             pages[f].IsIOInProgress(), pages[f].IsWriteInProgress(),
                             IsDirty(&pages[f]));
              }
            }
            last = now;
          }
        }) {}

  ~Progress() {
    {
      std::lock_guard lock(mutex_);
      stop_ = true;
    }
    cv_.notify_all();
    thread_.join();
  }

  void Tick() { done_.fetch_add(1, std::memory_order_relaxed); }
};

// =========================================================================
// 2. FIXTURE
//
//    THREAD SAFETY: PinReady() is safe from worker threads. NewPages(),
//    Evict() and ExpectPoolConsistent() are main-thread only.
// =========================================================================

class BufferPoolTest : public ::testing::Test {
protected:
  static constexpr const char *kDataDir = ".data_test_buffer_pool";
  static inline table_id next_tbl_ = 1;

  std::unique_ptr<DiskManagerAsync> dm_;
  std::unique_ptr<DiskScheduler> sched_;
  std::unique_ptr<PageVersionManager> vm_;
  std::unique_ptr<BufferPool> bp_;
  std::optional<HangGuard> hang_guard_;
  table_id tbl_ = 0;
  table_id churn_tbl_ = 0;

  /// Small by default, so tests reach the eviction path quickly.
  virtual frame_idx_t PoolPages() const { return 16; }

  static void SetUpTestSuite() {
    std::error_code ec;
    std::filesystem::remove_all(kDataDir, ec);
  }

  static void TearDownTestSuite() {
    if (::testing::Test::HasFailure()) { return; } // keep the files for inspection
    std::error_code ec;
    std::filesystem::remove_all(kDataDir, ec);
  }

  void SetUp() override {
    hang_guard_.emplace();
    dm_ = std::make_unique<DiskManagerAsync>(kDataDir);
    sched_ = std::make_unique<DiskScheduler>(dm_.get());
    sched_->Start();
    vm_ = std::make_unique<PageVersionManager>();
    bp_ = std::make_unique<BufferPool>(sched_.get(), vm_.get(), PoolPages());
    tbl_ = next_tbl_++;
    churn_tbl_ = next_tbl_++;

    // Only tbl_ needs a file: churn pages are clean and never reach the disk.
    // Starts at one page because fallocate() rejects a length of zero.
    dm_->CreateOpenFile(tbl_, 1);
  }

  /// Runs after every test, so each one also checks for leaks.
  void TearDown() override {
    ExpectPoolConsistent();
    sched_->Stop();
    // Newest first: RepeatedMissStorm calls SetUp() again, and replacing dm_ there
    // would otherwise destroy it while the old scheduler still points at it.
    bp_.reset();
    vm_.reset();
    sched_.reset();
    dm_.reset();
    hang_guard_.reset();
  }

  /// Creates n consecutive dirty pages, each holding StampFor(its id).
  std::vector<PageIdentifier> NewPages(u32 n, AccessStrategy strategy = {}) {
    std::vector<PageIdentifier> ids;
    ids.reserve(n);
    for (u32 i = 0; i < n; ++i) {
      Page *page = bp_->Reserve(tbl_, strategy);
      const PageIdentifier id = page->GetId();
      EXPECT_NE(id.pid, 0u) << "Pin() rejects pid 0";
      if (i > 0) { EXPECT_EQ(id.pid, ids.front().pid + i) << "pages must be consecutive"; }
      {
        PageWriteGuard guard(page);
        Store(page, StampFor(id));
      }
      bp_->Unpin(page, true);
      ids.push_back(id);
    }
    return ids;
  }

  /// Pushes every unpinned page of this test out of the pool, using throwaway
  /// clean pages from another table. It has to loop: frames still being written
  /// can't be claimed, so one sweep can finish while they're still cached.
  void Evict() {
    AccessStrategy strategy;
    Page *pages = bp_->GetPagesDebug();
    bool cached = true;
    while (cached) {
      for (frame_idx_t f = 0; f < PoolPages(); ++f) {
        bp_->Unpin(bp_->Reserve(churn_tbl_, strategy));
      }
      cached = false;
      for (frame_idx_t f = 0; f < PoolPages(); ++f) {
        if (pages[f].GetId().tbl_id == tbl_ && !IsPinned(&pages[f])) {
          pages[f].WaitWrite();
          cached = true;
        }
      }
    }
  }

  /// Pin() returns before the read finishes. Everyone must wait before use.
  Page *PinReady(PageIdentifier id) {
    Page *page = bp_->Pin(id, kKind);
    page->WaitIO();
    return page;
  }

  /// Checks the spec's end-of-run invariants: NoLeakedFlags, UniquePages and
  /// HashTableConsistent.
  void ExpectPoolConsistent() {
    Page *pages = bp_->GetPagesDebug();
    std::set<u64> seen;

    for (frame_idx_t f = 0; f < PoolPages(); ++f) {
      Page *page = &pages[f];
      page->WaitWrite();

      SCOPED_TRACE(f);
      EXPECT_EQ(PinsOf(page), 0u) << "leaked pin";
      EXPECT_FALSE(page->IsIOInProgress()) << "leaked IO_IN_PROGRESS";

      const PageIdentifier id = page->GetId();
      if (id == PageIdentifier{0}) { continue; }

      EXPECT_TRUE(seen.insert(StampFor(id)).second) << "page cached in two frames";
      if (PinsOf(page) == 0) {
        Page *found = bp_->Pin(id, kKind);
        EXPECT_EQ(found, page) << "hash table points at the wrong frame";
        found->WaitIO();
        bp_->Unpin(found);
      }
    }
  }
};

/// Room for strategies and scans, which need more than a handful of frames.
class BufferPoolLargeTest : public BufferPoolTest {
protected:
  frame_idx_t PoolPages() const override { return 256; }
};

/// The two frames TLC needed to find the pin bug.
class BufferPoolTinyTest : public BufferPoolTest {
protected:
  frame_idx_t PoolPages() const override { return 2; }
};

// =========================================================================
// 3. PINNING
// =========================================================================

TEST_F(BufferPoolTest, ReservedPageIsPinned) {
  AccessStrategy strategy;
  Page *page = bp_->Reserve(tbl_, strategy);

  EXPECT_EQ(PinsOf(page), 1u);
  EXPECT_FALSE(page->IsIOInProgress());
  bp_->Unpin(page);
}

TEST_F(BufferPoolTest, PinOfResidentPageReturnsSameFrame) {
  const PageIdentifier id = NewPages(1).front();

  Page *first = bp_->Pin(id, kKind);
  Page *second = bp_->Pin(id, kKind);

  EXPECT_EQ(first, second);
  EXPECT_EQ(first->GetId(), id);
  EXPECT_EQ(PinsOf(first), 2u);

  bp_->Unpin(first);
  bp_->Unpin(second);
  EXPECT_EQ(PinsOf(first), 0u);
}

TEST_F(BufferPoolTest, UnpinOnlyMarksDirtyWhenAsked) {
  const PageIdentifier id = NewPages(1).front();
  Evict();

  Page *page = PinReady(id);
  EXPECT_FALSE(IsDirty(page));
  bp_->Unpin(page, false);
  EXPECT_FALSE(IsDirty(page));

  page = PinReady(id);
  bp_->Unpin(page, true);
  EXPECT_TRUE(IsDirty(page));
}

// =========================================================================
// 4. EVICTION
// =========================================================================

/// The spec's UsePageCorrect: a pin stops a page from being replaced.
TEST_F(BufferPoolTest, PinnedPageIsNeverEvicted) {
  const PageIdentifier id = NewPages(1).front();
  Page *page = PinReady(id);

  Evict();

  EXPECT_EQ(page->GetId(), id);
  {
    PageReadGuard guard(page);
    EXPECT_EQ(Load(page), StampFor(id));
  }
  bp_->Unpin(page);
}

/// Needs a path that flushes dirty pages. With every frame dirty and no
/// flusher, this hangs and the watchdog aborts the run.
TEST_F(BufferPoolTest, DirtyPagesSurviveEviction) {
  const auto ids = NewPages(PoolPages() * 4);
  Evict();

  for (PageIdentifier id : ids) {
    SCOPED_TRACE(id.pid);
    Page *page = PinReady(id);
    EXPECT_EQ(page->GetId(), id);
    {
      PageReadGuard guard(page);
      EXPECT_EQ(Load(page), StampFor(id));
    }
    bp_->Unpin(page);
  }
}

/// The spec's FreshPage: each read sees the newest write, never an older copy
/// from disk.
TEST_F(BufferPoolTest, NewestVersionSurvivesRepeatedEviction) {
  const PageIdentifier id = NewPages(1).front();

  for (u64 version = 1; version <= 50; ++version) {
    SCOPED_TRACE(version);
    Page *page = PinReady(id);
    {
      PageWriteGuard guard(page);
      Store(page, version);
    }
    bp_->Unpin(page, true);

    Evict();

    page = PinReady(id);
    {
      PageReadGuard guard(page);
      EXPECT_EQ(Load(page), version);
    }
    bp_->Unpin(page);
  }
}

TEST_F(BufferPoolTest, PinWaitsForAFreeFrame) {
  const auto ids = NewPages(PoolPages() + 1);
  Evict();

  std::vector<Page *> pinned;
  for (frame_idx_t f = 0; f < PoolPages(); ++f) { pinned.push_back(PinReady(ids[f])); }

  std::atomic<bool> finished{false};
  std::thread waiter([&] {
    Page *page = PinReady(ids.back());
    finished = true;
    bp_->Unpin(page);
  });

  // Only ever fails if Pin() wrongly succeeds, so the sleep can't cause a flake.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(finished);

  bp_->Unpin(pinned.back());
  pinned.pop_back();
  waiter.join();
  EXPECT_TRUE(finished);

  for (Page *page : pinned) { bp_->Unpin(page); }
}

// =========================================================================
// 5. PIN RANGE
// =========================================================================

TEST_F(BufferPoolTest, PinRangeReadsColdPages) {
  constexpr u32 kN = 8;
  const auto ids = NewPages(kN);
  Evict();

  Page *out[kN];
  const PinRangeResult result = bp_->PinRange({ids.front(), kN}, out, kKind);

  EXPECT_EQ(result.count, kN);
  EXPECT_TRUE(result.io_started);
  for (u32 i = 0; i < result.count; ++i) {
    SCOPED_TRACE(i);
    out[i]->WaitIO();
    EXPECT_EQ(out[i]->GetId(), ids[i]);
    {
      PageReadGuard guard(out[i]);
      EXPECT_EQ(Load(out[i]), StampFor(ids[i]));
    }
    bp_->Unpin(out[i]);
  }
}

TEST_F(BufferPoolTest, PinRangeStopsAfterACachedPage) {
  constexpr u32 kN = 6;
  constexpr u32 kCached = 3;
  const auto ids = NewPages(kN);
  Evict();

  Page *cached = PinReady(ids[kCached]);

  Page *out[kN];
  const PinRangeResult result = bp_->PinRange({ids.front(), kN}, out, kKind);

  EXPECT_EQ(result.count, kCached + 1);
  EXPECT_TRUE(result.io_started);
  EXPECT_EQ(out[kCached], cached);

  for (u32 i = 0; i < result.count; ++i) {
    out[i]->WaitIO();
    bp_->Unpin(out[i]);
  }
  bp_->Unpin(cached);
}

TEST_F(BufferPoolTest, PinRangeOnCachedPageStartsNoIO) {
  const auto ids = NewPages(3);

  Page *out[3];
  const PinRangeResult result = bp_->PinRange({ids.front(), 3}, out, kKind);

  EXPECT_EQ(result.count, 1u);
  EXPECT_FALSE(result.io_started);
  EXPECT_EQ(out[0]->GetId(), ids.front());
  bp_->Unpin(out[0]);
}

// =========================================================================
// 6. ACCESS STRATEGIES
// =========================================================================

/// A bulk read must recycle its ring instead of pushing out the whole pool.
TEST_F(BufferPoolLargeTest, BulkReadStaysInsideItsRing) {
  const auto ids = NewPages(PoolPages() * 2);
  Evict();

  AccessStrategy strategy(BULK_READ);
  PinOptions opts(kKind, strategy);
  std::set<Page *> frames;

  for (PageIdentifier id : ids) {
    Page *page = bp_->Pin(id, opts);
    page->WaitIO();
    EXPECT_EQ(page->GetId(), id);
    frames.insert(page);
    bp_->Unpin(page);
  }

  EXPECT_LE(frames.size(), strategy.GetRingSize());
}

TEST_F(BufferPoolLargeTest, BulkReadDoesNotLoseDirtyPages) {
  const auto ids = NewPages(PoolPages() * 2);
  Evict();

  AccessStrategy strategy(BULK_READ);
  PinOptions opts(kKind, strategy);

  for (PageIdentifier id : ids) {
    Page *page = bp_->Pin(id, opts);
    page->WaitIO();
    {
      PageWriteGuard guard(page);
      Store(page, StampFor(id) + 1);
    }
    bp_->Unpin(page, true);
  }
  Evict();

  for (PageIdentifier id : ids) {
    SCOPED_TRACE(id.pid);
    Page *page = PinReady(id);
    {
      PageReadGuard guard(page);
      EXPECT_EQ(Load(page), StampFor(id) + 1);
    }
    bp_->Unpin(page);
  }
}

/// Checks data, not frame counts: how many frames a bulk write uses depends
/// on how fast the disk is, so an exact limit here would be flaky.
TEST_F(BufferPoolLargeTest, BulkWriteKeepsEveryPage) {
  const auto ids = NewPages(PoolPages() * 2, AccessStrategy(BULK_WRITE));
  Evict();

  for (PageIdentifier id : ids) {
    SCOPED_TRACE(id.pid);
    Page *page = PinReady(id);
    {
      PageReadGuard guard(page);
      EXPECT_EQ(Load(page), StampFor(id));
    }
    bp_->Unpin(page);
  }
}

// =========================================================================
// 7. CONCURRENCY
//
//    Same rules as the B-tree tests: end states are deterministic, and
//    workers report through ErrorLog because gtest asserts aren't safe there.
//
//    Metrics print right after the parallel phase, so the single-threaded
//    checks that follow don't count toward them.
// =========================================================================

/// Every thread misses on one page at once. Only one can win the hash insert,
/// and they must all land on the same frame (UniquePages).
TEST_F(BufferPoolTest, ConcurrentMissesShareOneFrame) {
  const unsigned T = DefaultThreads();
  constexpr int kRounds = 100;
  const PageIdentifier id = NewPages(1).front();

  {
    Progress progress(kRounds, bp_->GetPagesDebug(), PoolPages());

    for (int round = 0; round < kRounds; ++round) {
      Evict();

      ErrorLog log;
      std::vector<Page *> seen(T, nullptr);

      RunParallel(T, [&](unsigned t) {
        Page *page = PinReady(id);
        seen[t] = page;
        if (page->GetId() != id) { log.Failf("round ", round, ": wrong page"); }
        {
          PageReadGuard guard(page);
          if (Load(page) != StampFor(id)) { log.Failf("round ", round, ": wrong data"); }
        }
      });

      ExpectNoFailures(log);
      EXPECT_EQ(std::set<Page *>(seen.begin(), seen.end()).size(), 1u) << "round " << round;
      for (Page *page : seen) { bp_->Unpin(page); }
      progress.Tick();
    }
  }
  bp_->GetMonitorStats().Print();
}

/// Threads own separate pages but fight over frames. Reading back anything
/// but your last write means the pool returned stale or wrong data.
TEST_F(BufferPoolTest, ThreadsSeeTheirOwnWritesUnderEviction) {
  const unsigned T = DefaultThreads();
  constexpr u32 kPagesPerThread = 8;
  constexpr u32 kOps = 1'000;

  const auto ids = NewPages(T * kPagesPerThread);
  std::vector<u64> expected(ids.size());
  std::transform(ids.begin(), ids.end(), expected.begin(), StampFor);

  ErrorLog log;
  {
    Progress progress(T * kOps, bp_->GetPagesDebug(), PoolPages());
    RunParallel(T, [&](unsigned t) {
      std::mt19937 rng{0xD7u + t};
      std::uniform_int_distribution<u32> pick(t * kPagesPerThread, (t + 1) * kPagesPerThread - 1);

      for (u32 n = 0; n < kOps; ++n) {
        const u32 i = pick(rng);
        Page *page = PinReady(ids[i]);

        if (page->GetId() != ids[i]) { log.Failf("thread ", t, ": wrong page for ", i); }
        {
          PageWriteGuard guard(page);
          // The spec's StableDuringWrite. Reloading can't catch this: the dirty
          // flag rewrites the page before eviction, hiding the bad copy on disk.
          if (page->IsWriteInProgress()) { log.Failf("thread ", t, ": modifying during a write"); }
          if (Load(page) != expected[i]) { log.Failf("thread ", t, ": stale or torn page ", i); }
          Store(page, ++expected[i]);
        }
        bp_->Unpin(page, true);
        progress.Tick();
      }
    });
  }
  bp_->GetMonitorStats().Print();
  ExpectNoFailures(log);

  Evict();
  for (size_t i = 0; i < ids.size(); ++i) {
    SCOPED_TRACE(i);
    Page *page = PinReady(ids[i]);
    {
      PageReadGuard guard(page);
      EXPECT_EQ(Load(page), expected[i]);
    }
    bp_->Unpin(page);
  }
}

/// All threads increment shared counters. The final total can only be right
/// if no update was lost across eviction, reload or a mid-flight write.
TEST_F(BufferPoolTest, SharedCountersLoseNoUpdates) {
  const unsigned T = DefaultThreads();
  constexpr u32 kPages = 48;
  constexpr u32 kOps = 1'000;

  const auto ids = NewPages(kPages);

  ErrorLog log;
  {
    Progress progress(T * kOps, bp_->GetPagesDebug(), PoolPages());
    RunParallel(T, [&](unsigned t) {
      std::mt19937 rng{0xBEEFu + t};
      std::uniform_int_distribution<u32> pick(0, kPages - 1);

      for (u32 n = 0; n < kOps; ++n) {
        const PageIdentifier id = ids[pick(rng)];
        Page *page = PinReady(id);
        if (page->GetId() != id) { log.Failf("thread ", t, ": wrong page"); }
        {
          PageWriteGuard guard(page);
          if (page->IsWriteInProgress()) { log.Failf("thread ", t, ": modifying during a write"); }
          const auto value = Load(page);
          if (!value) {
            log.Failf("thread ", t, ": torn page");
          } else {
            Store(page, *value + 1);
          }
        }
        bp_->Unpin(page, true);
        progress.Tick();
      }
    });
  }
  bp_->GetMonitorStats().Print();
  ExpectNoFailures(log);

  Evict();
  u64 total = 0;
  for (PageIdentifier id : ids) {
    Page *page = PinReady(id);
    {
      PageReadGuard guard(page);
      const auto value = Load(page);
      ASSERT_TRUE(value.has_value());
      total += *value - StampFor(id);
    }
    bp_->Unpin(page);
  }
  EXPECT_EQ(total, static_cast<u64>(T) * kOps);
}

/// The stress version of the pin race TLC found. More pages than frames keeps
/// threads claiming and losing, and a thread left pinned to the wrong page shows
/// up as a wrong ID. The window is tiny, so a pass here is weak evidence: the
/// spec is what rules this race out.
TEST_F(BufferPoolTinyTest, TinyPoolNeverReturnsTheWrongPage) {
  const unsigned T = std::max(3u, DefaultThreads());
  constexpr u32 kPages = 4;
  constexpr u32 kOps = 5'000;

  const auto ids = NewPages(kPages);

  ErrorLog log;
  {
    Progress progress(T * kOps, bp_->GetPagesDebug(), PoolPages());
    RunParallel(T, [&](unsigned t) {
      std::mt19937 rng{0xC0FFEEu + t};
      std::uniform_int_distribution<u32> pick(0, kPages - 1);

      for (u32 n = 0; n < kOps; ++n) {
        const PageIdentifier id = ids[pick(rng)];
        Page *page = PinReady(id);
        if (page->GetId() != id) { log.Failf("thread ", t, ": wrong page"); }
        {
          PageReadGuard guard(page);
          if (Load(page) != StampFor(id)) { log.Failf("thread ", t, ": wrong data"); }
        }
        bp_->Unpin(page);
        progress.Tick();
      }
    });
  }
  bp_->GetMonitorStats().Print();
  ExpectNoFailures(log);
}

/// Overlapping scans and point pins, which covers the path where one thread
/// waits on a read another thread started. Range size times thread count must
/// stay below the pool size: see DISABLED_PinRangeDoesNotDeadlockOnASmallPool.
TEST_F(BufferPoolLargeTest, ScansAndPinsShareReads) {
  const unsigned T = DefaultThreads();
  constexpr u32 kRange = 4;
  constexpr u32 kOps = 2'000;
  ASSERT_LT(T * kRange, PoolPages());

  const auto ids = NewPages(PoolPages() * 2);

  ErrorLog log;
  {
    Progress progress(T * kOps, bp_->GetPagesDebug(), PoolPages());
    RunParallel(T, [&](unsigned t) {
      std::mt19937 rng{0xABCDu + t};
      std::uniform_int_distribution<size_t> pick(0, ids.size() - kRange);

      for (u32 n = 0; n < kOps; ++n) {
        const size_t first = pick(rng);
        Page *out[kRange];
        const PinRangeResult result = bp_->PinRange({ids[first], kRange}, out, kKind);

        for (u32 i = 0; i < result.count; ++i) {
          out[i]->WaitIO();
          if (out[i]->GetId() != ids[first + i]) { log.Failf("thread ", t, ": wrong page"); }
          {
            PageReadGuard guard(out[i]);
            if (Load(out[i]) != StampFor(ids[first + i])) {
              log.Failf("thread ", t, ": wrong data");
            }
          }
          bp_->Unpin(out[i]);
        }
        progress.Tick();
      }
    });
  }
  bp_->GetMonitorStats().Print();
  ExpectNoFailures(log);
}

/// Bulk readers running over the same pages writers are updating.
TEST_F(BufferPoolLargeTest, BulkReadersRaceWithWriters) {
  const unsigned T = DefaultThreads();
  const unsigned writers = std::max(1u, T / 2);
  const unsigned readers = std::max(1u, T - writers);
  constexpr u32 kOps = 3'000;

  const auto ids = NewPages(PoolPages() * 2);

  ErrorLog log;
  {
    const u64 work = static_cast<u64>(writers) * kOps + static_cast<u64>(readers) * ids.size();
    Progress progress(work, bp_->GetPagesDebug(), PoolPages());
    RunParallelMixed(
        writers,
        [&](unsigned t) {
          std::mt19937 rng{0x1234u + t};
          std::uniform_int_distribution<size_t> pick(0, ids.size() - 1);
          for (u32 n = 0; n < kOps; ++n) {
            const PageIdentifier id = ids[pick(rng)];
            Page *page = PinReady(id);
            {
              PageWriteGuard guard(page);
              const auto value = Load(page);
              if (!value) {
                log.Failf("writer ", t, ": torn page");
              } else {
                Store(page, *value + 1);
              }
            }
            bp_->Unpin(page, true);
            progress.Tick();
          }
        },
        readers,
        [&](unsigned t) {
          AccessStrategy strategy(BULK_READ);
          PinOptions opts(kKind, strategy);
          for (size_t i = 0; i < ids.size(); ++i) {
            Page *page = bp_->Pin(ids[i], opts);
            page->WaitIO();
            if (page->GetId() != ids[i]) { log.Failf("reader ", t, ": wrong page"); }
            {
              PageReadGuard guard(page);
              if (!Load(page)) { log.Failf("reader ", t, ": torn page"); }
            }
            bp_->Unpin(page);
            progress.Tick();
          }
        });
  }
  bp_->GetMonitorStats().Print();
  ExpectNoFailures(log);
}

/// A race that fires one run in fifty passes a single run, so this one is
/// small enough to repeat. Calls TearDown/SetUp for a fresh pool each pass.
TEST_F(BufferPoolTest, RepeatedMissStorm) {
  constexpr int kIterations = 20;
  constexpr u32 kOps = 500;
  const unsigned T = DefaultThreads();

  Repeat(kIterations, [&](int iteration) {
    TearDown();
    SetUp();

    const auto ids = NewPages(PoolPages() * 2);

    ErrorLog log;
    {
      Progress progress(T * kOps, bp_->GetPagesDebug(), PoolPages());
      RunParallel(T, [&](unsigned t) {
        std::mt19937 rng{static_cast<u32>(iteration) * 31u + t};
        std::uniform_int_distribution<size_t> pick(0, ids.size() - 1);
        for (u32 n = 0; n < kOps; ++n) {
          const PageIdentifier id = ids[pick(rng)];
          Page *page = PinReady(id);
          if (page->GetId() != id) { log.Failf("iter ", iteration, ": wrong page"); }
          bp_->Unpin(page);
          progress.Tick();
        }
      });
    }
    ExpectNoFailures(log);
  });

  // Every pass gets a new pool, so this covers the last pass only.
  bp_->GetMonitorStats().Print();
}

// =========================================================================
// 8. PENDING
//
//    The first two need a way to make Enqueue() fail. The third hangs until
//    PinRange can submit what it has when no frame is free.
// =========================================================================

TEST_F(BufferPoolTest, DISABLED_FailedReadLeavesNoTableEntry) {
  GTEST_SKIP() << "needs fault injection in the disk scheduler";
}

TEST_F(BufferPoolTest, DISABLED_FailedPinRangeReleasesItsPins) {
  GTEST_SKIP() << "needs fault injection in the disk scheduler";
}

TEST_F(BufferPoolTest, DISABLED_PinRangeDoesNotDeadlockOnASmallPool) {
  GTEST_SKIP() << "ranges larger than the free frames currently hang";
}

} // namespace