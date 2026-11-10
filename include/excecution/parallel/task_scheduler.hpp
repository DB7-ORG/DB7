#pragma once

#include "shared/data_structures/token_queue.hpp"
#include <vector>

namespace db7 {

namespace execution::parallel {

enum class TaskExecutionMode : u8 { PROCESS_ALL };
enum class TaskExecutionResult : u8 { Finished, Error };
struct Task {
  virtual TaskExecutionResult Execute(TaskExecutionMode mode) = 0;
};

class TaskSchedulerPool;

class TaskScheduler {
private:
  std::unique_ptr<TaskSchedulerPool> pool_;
  shared::TokenQueue<std::shared_ptr<Task>> queue_;
  std::atomic<u32> tasks_in_queue_{0};

public:
  TaskScheduler();
  ~TaskScheduler();

  u32 GetTasksInQueue() const;
  void ScheduleTask(std::shared_ptr<Task> task);
  void ScheduleTasks(std::vector<std::shared_ptr<Task>> &tasks);
  bool GetTask(std::shared_ptr<Task> &task);
  void Signal(idx_t n);
  bool ProcessTask(std::shared_ptr<Task> &task);

  void ExecuteForever(std::atomic<bool> *marker);
};
} // namespace execution::parallel
} // namespace db7