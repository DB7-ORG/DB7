#pragma once

#include "common.hpp"
#include "execution/sources/table_local_state.hpp"
#include "shared/sync/waiter_list.hpp"
#include <memory>

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
  std::unique_ptr<SourceLocalState> source_local_;
  void *code_;

  Task() = default;
  Task(Pipeline *p, QueryContext *q, TaskScheduler *s);

  void Init(Pipeline *p, QueryContext *q, TaskScheduler *s);

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;
  TaskExecutionResult Execute(TaskExecutionMode mode);
};
} // namespace parallel
} // namespace execution
} // namespace db7