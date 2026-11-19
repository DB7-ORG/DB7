#include "execution/parallel/exec_task.hpp"
#include "execution/parallel/task_scheduler.hpp"
#include "execution/pipeline.hpp"
#include "execution/query_context.hpp"
#include "execution/sources/table_source.hpp"

namespace db7::execution::parallel {

Task::Task(Pipeline *p, QueryContext *q, TaskScheduler *s) { Init(p, q, s); }

void Task::Init(Pipeline *p, QueryContext *q, TaskScheduler *s) {
  DB7_ASSERT(!source_local_, "Task initialized twice");
  wake_fn = [](shared::Waiter *w) { // captureless lambda converts to a function pointer
    auto *t = static_cast<Task *>(w);
    t->scheduler->ScheduleTask(t); // resume: same task, NOT a new one, so no counter change
  };
  pipeline = p;
  query = q;
  scheduler = s;
  auto *src = p->State().source;
  // auto &table_src = src->Cast<TableSource>();
  source_local_ = src->InitLocal();
  code_ = p->State().code;
}

TaskExecutionResult Task::Execute(TaskExecutionMode mode) {
  (void)mode;

  using Clock = std::chrono::steady_clock;

  // start deadline timer
  const auto deadline = Clock::now() + yield_time_;

  // while deadline or no tuples
  auto source = pipeline->State().source;
  SourceLocalState &local = *source_local_;
  do {
    if (query->IsCancelled()) return TaskExecutionResult::Error;

    // TODO does this virtual cost add up
    // consider making task abstract (this will reduce virtual calls to 1)
    auto result = source->Next(local);
    switch (result) {
    case PollResult::Finished: return TaskExecutionResult::Finished;
    case PollResult::HaveMore: // do something w data
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      (void)code_;
      break;
    case PollResult::Blocked: return TaskExecutionResult::Wait;
    }
  } while (Clock::now() < deadline);

  return TaskExecutionResult::Timed;
}

void Task::WaitForSource() { pipeline->Context()->source->Wait(*source_local_); }

} // namespace db7::execution::parallel