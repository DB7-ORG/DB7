#include <gtest/gtest.h>

#include "execution/executor.hpp" // ADAPT: wherever Executor / Completion live
#include "execution/parallel/exec_task.hpp"
#include "execution/parallel/task_scheduler.hpp"
#include "execution/pipeline.hpp"
#include "execution/query_context.hpp"
#include "execution/sources/table_source.hpp" // BaseSource, SourceLocalState

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

using namespace db7;
using namespace db7::execution;
using namespace db7::execution::parallel;
using namespace std::chrono_literals;

namespace {

// =========================================================================
// 1. FAKE SOURCE
// =========================================================================

// SourceType only has TABLE. An unscoped enum with no fixed underlying type
// can still hold 1 (its valid range is 0..1), so the fake gets a tag no real
// source uses. A leftover Cast<TableSource>() / Cast<TableLocalState>() in the
// code under test then fails its assert instead of silently misreading the
// fake. If SourceType ever gets a second enumerator, add a real FAKE value.
constexpr SourceType kFakeType = static_cast<SourceType>(1);

constexpr auto kMode = TaskExecutionMode::PROCESS_ALL;

struct FakeLocal final : SourceLocalState {
  static constexpr SourceType kType = kFakeType;
  FakeLocal() : SourceLocalState(kType) {}

  size_t pos = 0; // next script step; only the task owning this state touches it
};

/// Every task's local state replays the same script. Past its end, Next()
/// returns Finished. `work_per_chunk` simulates the cost of processing a chunk.
class FakeSource final : public BaseSource {
public:
  static constexpr SourceType kType = kFakeType;

  explicit FakeSource(std::vector<PollResult> script,
                      std::chrono::microseconds work_per_chunk = 0us)
      : BaseSource(kType), script_(std::move(script)), work_(work_per_chunk) {}

  std::unique_ptr<SourceLocalState> InitLocal() override {
    locals++;
    return std::make_unique<FakeLocal>();
  }

  SourceResult Next(SourceLocalState &l) override {
    auto &st = l.Cast<FakeLocal>();
    calls++;
    if (st.pos >= script_.size()) {
      finished++;
      return PollResult::Finished;
    }
    const PollResult r = script_[st.pos++];
    if (r == PollResult::HaveMore) {
      chunks++;
      if (work_ > 0us) std::this_thread::sleep_for(work_);
    }
    if (r == PollResult::Blocked) blocked++;
    return r;
  }

  void Wait(SourceLocalState &l) override {
    (void)l.Cast<FakeLocal>(); // asserts the task passed its own state
    waits++;
  }

  std::atomic<int> locals = 0;   // InitLocal() calls
  std::atomic<int> calls = 0;    // every Next() call
  std::atomic<int> chunks = 0;   // HaveMore returned
  std::atomic<int> blocked = 0;  // Blocked returned
  std::atomic<int> waits = 0;    // Wait() calls
  std::atomic<int> finished = 0; // Finished returned

private:
  const std::vector<PollResult> script_;
  const std::chrono::microseconds work_;
};

std::vector<PollResult> Repeat(PollResult r, size_t n) { return std::vector<PollResult>(n, r); }

constexpr auto H = PollResult::HaveMore;
constexpr auto B = PollResult::Blocked;

/// Polls `done` until it's true or `timeout` passes. Scheduler bugs show up
/// as hangs, so every wait has a bound.
bool WaitUntil(const std::function<bool()> &done, std::chrono::milliseconds timeout = 5s) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!done()) {
    if (std::chrono::steady_clock::now() > deadline) return false;
    std::this_thread::sleep_for(1ms);
  }
  return true;
}

// =========================================================================
// 2. FIXTURES
// =========================================================================

class TaskTest : public ::testing::Test {
protected:
  QueryContext query_;
  std::unique_ptr<TaskScheduler> scheduler_; // the executor's scheduler, also runs SchedulerTest
  std::unique_ptr<Executor> executor_;       // only here because Pipeline needs one
  std::unique_ptr<Pipeline> pipeline_;

  void SetUp() override {
    scheduler_ = std::make_unique<TaskScheduler>();
    // No-op completion: Completion::operator() calls fn without a null check.
    executor_ = std::make_unique<Executor>(query_, *scheduler_, Completion{[](void *) {}, nullptr});
    pipeline_ = std::make_unique<Pipeline>(*executor_, std::make_unique<PipelineContext>());
  }

  // Stop the workers before any task, pipeline or source is destroyed: a
  // worker may still be running a task when the test body returns.
  void TearDown() override { scheduler_.reset(); }

  std::unique_ptr<Task> MakeTask(FakeSource &source) {
    pipeline_->State().source = &source; // ADAPT if `source` isn't a raw pointer
    return std::make_unique<Task>(pipeline_.get(), &query_, scheduler_.get()); // runs Init
  }
};

class SchedulerTest : public TaskTest {
protected:
  std::vector<std::unique_ptr<Task>> tasks_;

  /// Creates `n` tasks over `source` and hands them all to the scheduler.
  void Run(FakeSource &source, int n) {
    std::vector<TaskQObj> queue;
    for (int i = 0; i < n; ++i) {
      tasks_.push_back(MakeTask(source));
      queue.push_back(tasks_.back().get());
    }
    scheduler_->ScheduleTasks(queue);
  }
};

// =========================================================================
// 3. TASK::EXECUTE (single thread, no scheduler)
// =========================================================================

TEST_F(TaskTest, InitCreatesOneLocalStatePerTask) {
  FakeSource source({});
  auto a = MakeTask(source);
  auto b = MakeTask(source);
  EXPECT_EQ(source.locals.load(), 2);
}

TEST_F(TaskTest, RunsSourceUntilFinished) {
  FakeSource source(Repeat(H, 3));
  auto task = MakeTask(source);

  EXPECT_EQ(task->Execute(kMode), TaskExecutionResult::Finished);
  EXPECT_EQ(source.chunks.load(), 3);
  EXPECT_EQ(source.finished.load(), 1);
}

TEST_F(TaskTest, EmptySourceFinishesImmediately) {
  FakeSource source({});
  auto task = MakeTask(source);

  EXPECT_EQ(task->Execute(kMode), TaskExecutionResult::Finished);
  EXPECT_EQ(source.calls.load(), 1);
}

TEST_F(TaskTest, BlockedReturnsWaitAndResumesWhereItStopped) {
  FakeSource source({H, B, H});
  auto task = MakeTask(source);

  EXPECT_EQ(task->Execute(kMode), TaskExecutionResult::Wait);
  EXPECT_EQ(source.chunks.load(), 1);

  EXPECT_EQ(task->Execute(kMode), TaskExecutionResult::Finished);
  EXPECT_EQ(source.chunks.load(), 2);
  EXPECT_EQ(source.calls.load(), 4); // H, B, H, Finished: nothing replayed or skipped
}

TEST_F(TaskTest, TimeSliceEndsWithTimedAndResumes) {
  // 200 chunks x 100 us = ~20 ms, several 5 ms slices.
  FakeSource source(Repeat(H, 200), 100us);
  auto task = MakeTask(source);

  EXPECT_EQ(task->Execute(kMode), TaskExecutionResult::Timed);
  EXPECT_GT(source.chunks.load(), 0);
  EXPECT_LT(source.chunks.load(), 200);

  TaskExecutionResult res = TaskExecutionResult::Timed;
  for (int slices = 0; slices < 1000 && res == TaskExecutionResult::Timed; ++slices) {
    res = task->Execute(kMode);
  }
  EXPECT_EQ(res, TaskExecutionResult::Finished);
  EXPECT_EQ(source.chunks.load(), 200);
}

TEST_F(TaskTest, CancelledQueryStopsBeforeReadingSource) {
  FakeSource source(Repeat(H, 3));
  auto task = MakeTask(source);
  query_.Cancel();

  EXPECT_EQ(task->Execute(kMode), TaskExecutionResult::Error);
  EXPECT_EQ(source.calls.load(), 0);
}

// =========================================================================
// 4. SCHEDULER (worker threads)
// =========================================================================

TEST_F(SchedulerTest, RunsEveryTaskToCompletion) {
  FakeSource source(Repeat(H, 5));
  Run(source, 16);

  ASSERT_TRUE(WaitUntil([&] { return source.finished == 16; }))
      << "finished: " << source.finished.load() << " of 16";
  EXPECT_EQ(source.chunks.load(), 16 * 5);
}

TEST_F(SchedulerTest, BlockedTasksAreWaitedOnAndRescheduled) {
  FakeSource source({H, B, H, B, H});
  Run(source, 8);

  ASSERT_TRUE(WaitUntil([&] { return source.finished == 8; }))
      << "finished: " << source.finished.load() << " of 8 (a Wait task was dropped?)";
  EXPECT_EQ(source.chunks.load(), 8 * 3);
  EXPECT_EQ(source.blocked.load(), 8 * 2);
  EXPECT_EQ(source.waits.load(), 8 * 2);
}

TEST_F(SchedulerTest, TimedOutTasksAreRescheduled) {
  // ~10 ms per task, so every task runs out of at least one 5 ms slice.
  FakeSource source(Repeat(H, 100), 100us);
  Run(source, 8);

  ASSERT_TRUE(WaitUntil([&] { return source.finished == 8; }))
      << "finished: " << source.finished.load() << " of 8 (a Timed task was dropped?)";
  EXPECT_EQ(source.chunks.load(), 8 * 100);
}

TEST_F(SchedulerTest, MoreTasksThanThreads) {
  FakeSource source({H, B, H});
  const int n = 64; // well above any worker count, so tasks queue up
  Run(source, n);

  ASSERT_TRUE(WaitUntil([&] { return source.finished == n; }))
      << "finished: " << source.finished.load() << " of " << n;
  EXPECT_EQ(source.chunks.load(), n * 2);
}

TEST_F(SchedulerTest, CancellingStopsEveryTask) {
  // Would run for minutes if nothing stopped it.
  FakeSource source(Repeat(H, 1'000'000), 100us);
  Run(source, 8);

  ASSERT_TRUE(WaitUntil([&] { return source.chunks > 0; }));
  query_.Cancel();

  // Each task notices within one chunk; after that nobody calls Next() again.
  std::this_thread::sleep_for(50ms);
  const int calls = source.calls.load();
  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(source.calls.load(), calls) << "a task kept reading after cancel";
  EXPECT_EQ(source.finished.load(), 0);
}

} // namespace