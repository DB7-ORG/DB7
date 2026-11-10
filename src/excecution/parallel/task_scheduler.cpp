#include "excecution/parallel/task_scheduler.hpp"
#include "excecution/parallel/task_scheduler_pool.hpp"
#include "shared/data_structures/token_queue.hpp"

namespace db7::execution::parallel {
TaskScheduler::TaskScheduler() {
  pool_ = std::make_unique<TaskSchedulerPool>();
  pool_->Reload(*this);
}

u32 TaskScheduler::GetTasksInQueue() const { return tasks_in_queue_; }

void TaskScheduler::ScheduleTask(std::shared_ptr<Task> task) {
  queue_.Enqueue(std::move(task));
  Signal(1);
}

void TaskScheduler::ScheduleTasks(std::vector<std::shared_ptr<Task>> &tasks) {
  queue_.EnqueueBulk(std::make_move_iterator(tasks.begin()), tasks.size());
  Signal(tasks.size());
}

bool TaskScheduler::GetTask(std::shared_ptr<Task> &task) {
  if (queue_.TryDequeue(task)) { return true; }
  return false;
}

void TaskScheduler::Signal(idx_t n) { pool_->Signal(n); }

bool TaskScheduler::ProcessTask(std::shared_ptr<Task> &task) {
  if (GetTask(task)) {
    auto execute_result = task->Execute(TaskExecutionMode::PROCESS_ALL);
    (void)execute_result; // TODO scheduler process result and stuff
    return true;
  }
  return false;
}

void TaskScheduler::ExecuteForever(std::atomic<bool> *marker) {
  std::shared_ptr<Task> task;
  while (*marker) { ProcessTask(task); }
}

} // namespace db7::execution::parallel