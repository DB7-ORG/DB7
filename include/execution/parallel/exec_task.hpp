#pragma once

#include "common.hpp"
#include "shared/sync/waiter_list.hpp"

namespace db7 {
namespace execution {
class Pipeline;
class QueryContext;

namespace parallel {

class TaskScheduler;

enum class TaskExecutionMode : u8 { PROCESS_ALL };
enum class TaskExecutionResult : u8 { Finished, Timed, Error, Wait };
struct Task : shared::Waiter {
  Pipeline *pipeline = nullptr;
  QueryContext *query = nullptr;
  TaskScheduler *scheduler = nullptr;

  Task() = default;
  Task(Pipeline *p, QueryContext *q, TaskScheduler *s);

  void Init(Pipeline *p, QueryContext *q, TaskScheduler *s) {
    pipeline = p;
    query = q;
    scheduler = s;
  }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;
  TaskExecutionResult Execute(TaskExecutionMode mode);
};
} // namespace parallel
} // namespace execution
} // namespace db7