#pragma once

#include "execution/parallel/exec_task.hpp"
#include "shared/data_structures/token_queue.hpp"

#include <memory>
#include <span>
#include <vector>

namespace db7 {

namespace execution::parallel {

class TaskScheduler;
class TaskSchedulerPool;
using TaskQObj = Task *;

class TaskScheduler {
private:
  std::unique_ptr<TaskSchedulerPool> pool_;
  shared::TokenQueue<TaskQObj> queue_;
  std::atomic<u32> tasks_in_queue_{0};

public:
  TaskScheduler();
  ~TaskScheduler();

  u32 NumberOfThreads() const;
  u32 GetTasksInQueue() const;

  void ScheduleTask(TaskQObj task);
  void ScheduleTasks(std::vector<TaskQObj> &tasks);
  void ScheduleTasks(std::span<Task> tasks);
  bool GetTask(TaskQObj &task);
  void Signal(idx_t n);
  bool ProcessTask(TaskQObj &task);

  void ExecuteForever(std::atomic<bool> *marker);
};
} // namespace execution::parallel
} // namespace db7