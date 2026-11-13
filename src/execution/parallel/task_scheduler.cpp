#include "execution/parallel/task_scheduler.hpp"
#include "execution/parallel/task_scheduler_pool.hpp"
#include "execution/pipeline.hpp"
#include "execution/query_context.hpp"
#include "shared/data_structures/token_queue.hpp"

#include <ranges>

namespace db7::execution::parallel {
TaskScheduler::TaskScheduler() {
  pool_ = std::make_unique<TaskSchedulerPool>();
  pool_->Reload(*this);
}

u32 TaskScheduler::NumberOfThreads() const { return pool_->NumberOfThreads(); }

u32 TaskScheduler::GetTasksInQueue() const { return tasks_in_queue_; }

void TaskScheduler::ScheduleTask(TaskQObj task) {
  queue_.Enqueue(task);
  Signal(1);
}

void TaskScheduler::ScheduleTasks(std::vector<TaskQObj> &tasks) {
  queue_.EnqueueBulk(std::make_move_iterator(tasks.begin()), tasks.size());
  Signal(tasks.size());
}

void TaskScheduler::ScheduleTasks(std::span<Task> tasks) {
  auto ptrs = tasks | std::views::transform([](Task &t) { return &t; });
  queue_.EnqueueBulk(ptrs.begin(), tasks.size());
  Signal(tasks.size());
}

bool TaskScheduler::GetTask(TaskQObj &task) {
  if (queue_.TryDequeue(task)) { return true; }
  return false;
}

void TaskScheduler::Signal(idx_t n) { pool_->Signal(n); }

bool TaskScheduler::ProcessTask(TaskQObj &task) {
  if (!GetTask(task)) { return false; }
  switch (task->Execute(TaskExecutionMode::PROCESS_ALL)) {
  case TaskExecutionResult::Error: {
    task->query->Cancel();
    [[fallthrough]];
  }
  case TaskExecutionResult::Finished: {
    Pipeline *p = task->pipeline;
    p->TaskDone();
    break;
  }
  case TaskExecutionResult::Timed: {
    ScheduleTask(task);
    break;
  }
  case TaskExecutionResult::Wait: {
    break;
  }
  }
  return true;
}

void TaskScheduler::ExecuteForever(std::atomic<bool> *marker) {
  TaskQObj task;
  while (true) {
    pool_->Wait();
    if (!marker->load(std::memory_order_relaxed)) return;
    ProcessTask(task);
  }
}

} // namespace db7::execution::parallel