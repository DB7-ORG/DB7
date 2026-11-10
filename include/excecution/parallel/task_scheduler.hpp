#pragma once

#include "excecution/pipeline.hpp"
#include "excecution/query_context.hpp"
#include "shared/data_structures/token_queue.hpp"
#include <vector>

namespace db7 {

namespace execution::parallel {

enum class TaskExecutionMode : u8 { PROCESS_ALL };
enum class TaskExecutionResult : u8 { Finished, Error };
struct Task {
  Pipeline *pipeline;
  QueryContext *query;

  Task() {}
  Task(Pipeline *p, QueryContext *q) : pipeline(p), query(q) {}

  TaskExecutionResult Execute(TaskExecutionMode mode) {
    (void)mode;
    pipeline->TaskDone();
    return TaskExecutionResult::Finished;
  }
};

class TaskSchedulerPool;

using TaskQObj = Task;

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
  bool GetTask(TaskQObj &task);
  void Signal(idx_t n);
  bool ProcessTask(TaskQObj &task);

  void ExecuteForever(std::atomic<bool> *marker);
};
} // namespace execution::parallel
} // namespace db7