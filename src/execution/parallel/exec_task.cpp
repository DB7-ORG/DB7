#include "execution/parallel/exec_task.hpp"
#include "execution/parallel/task_scheduler.hpp"
#include "execution/pipeline.hpp"
#include "execution/query_context.hpp"
#include "execution/sources/table_source.hpp"

namespace db7::execution::parallel {
Task::Task(Pipeline *p, QueryContext *q, TaskScheduler *s) : pipeline(p), query(q), scheduler(s) {
  wake_fn = [](shared::Waiter *w) { // captureless lambda converts to a function pointer
    auto *t = static_cast<Task *>(w);
    t->scheduler->ScheduleTask(t); // resume: same task, NOT a new one, so no counter change
  };
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
    if (query->IsCancelled()) return TaskExecutionResult::Finished;
    // process data
    //  if blocked page wait
    std::cout << "completed task" << std::endl;

    return TaskExecutionResult::Finished; // TODO

  } while (Clock::now() < deadline);

  return TaskExecutionResult::Timed;
}

} // namespace db7::execution::parallel