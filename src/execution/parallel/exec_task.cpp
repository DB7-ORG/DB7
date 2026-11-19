#include "execution/parallel/exec_task.hpp"
#include "execution/parallel/task_scheduler.hpp"
#include "execution/pipeline.hpp"
#include "execution/query_context.hpp"
#include "execution/sources/table_source.hpp"

namespace db7::execution::parallel {

// TODO test
static std::atomic<u64> task_num = 0;

Task::Task(Pipeline *p, QueryContext *q, TaskScheduler *s) : pipeline(p), query(q), scheduler(s) {
  wake_fn = [](shared::Waiter *w) { // captureless lambda converts to a function pointer
    auto *t = static_cast<Task *>(w);
    t->scheduler->ScheduleTask(t); // resume: same task, NOT a new one, so no counter change
  };

  task_num++;
}

void Task::Init(Pipeline *p, QueryContext *q, TaskScheduler *s) {
  pipeline = p;
  query = q;
  scheduler = s;
  auto src = p->State().source;
  auto &table_src = src->Cast<TableSource>();
  source_local_ = table_src.InitLocal();
  code_ = p->State().code;
}

TaskExecutionResult Task::Execute(TaskExecutionMode mode) {
  (void)mode;

  using Clock = std::chrono::steady_clock;
  static constexpr auto kTimeSlice = std::chrono::milliseconds(5);

  // start deadline timer
  const auto deadline = Clock::now() + kTimeSlice;

  // while deadline or no tuples
  do {
    if (query->IsCancelled()) return TaskExecutionResult::Error;

    auto &state = source_local_->Cast<TableLocalState>();
    auto source = pipeline->State().source;
    auto result = source->Next(state, nullptr);
    switch (result) {
    case PollResult::Finished:
      std::cout << "completed task: " << task_num << std::endl;
      return TaskExecutionResult::Finished;
    case PollResult::HaveMore: // do something w data
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      (void)code_;
      break;
    case PollResult::Blocked: return TaskExecutionResult::Wait;
    }
  } while (Clock::now() < deadline);

  return TaskExecutionResult::Timed;
}

} // namespace db7::execution::parallel