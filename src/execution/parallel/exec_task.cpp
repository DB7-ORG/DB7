#include "execution/parallel/exec_task.hpp"
#include "execution/parallel/task_scheduler.hpp"
#include "execution/pipeline.hpp"
#include "execution/query_context.hpp"

namespace db7::execution::parallel {
Task::Task(Pipeline *p, QueryContext *q, TaskScheduler *s) : pipeline(p), query(q), scheduler(s) {
  wake_fn = [](shared::Waiter *w) { // captureless lambda converts to a function pointer
    auto *t = static_cast<Task *>(w);
    t->scheduler->ScheduleTask(t); // resume: same task, NOT a new one, so no counter change
  };
}

TaskExecutionResult Task::Execute(TaskExecutionMode mode) {
  (void)mode;
  // start doing some work
  if (false) { return TaskExecutionResult::Error; } // error, abort query

  return TaskExecutionResult::Finished;
}

} // namespace db7::execution::parallel