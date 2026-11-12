#include "excecution/executor.hpp"
#include "excecution/parallel/task_scheduler.hpp"
#include "excecution/pipeline.hpp"
#include "excecution/query_context.hpp"
#include "shared/macro_helper.hpp"

#include <algorithm>

namespace db7::execution {

Executor::Executor(QueryContext &q, parallel::TaskScheduler &s, Completion on_done, void *physical)
    : query_(q), scheduler_(s), on_done_(on_done) {
  DB7_ASSERT(static_cast<bool>(on_done_), "Completion callback required");

  (void)physical;
  // TODO create pipelines from physical plan
}

Executor::~Executor() = default;

void Executor::Start() {
  if (SchedulePipeline()) return;
  DB7_ASSERT(pipelines_.empty(), "Pipelines exist but none is runnable (cycle?)");
  Finish();
}

void Executor::OnPipelineFinished() noexcept {
  if (SchedulePipeline()) return;
  DB7_ASSERT(
      std::all_of(pipelines_.begin(), pipelines_.end(), [](auto &p) { return p->state_ == DONE; }),
      "No runnable pipeline but some never ran (cycle or bad DependsOn?)");
  Finish();
}

void Executor::Finish() {
  Completion done = on_done_; // copy out first: the callback may destroy *this
  done();                     // must be the last thing touching this executor
}

bool Executor::SchedulePipeline() {
  for (auto &pipeline : pipelines_) {
    if (pipeline->Available()) {
      SchedulePipeline(*pipeline);
      return true;
    }
  }
  return false;
}

void Executor::SchedulePipeline(Pipeline &pipeline) {
  auto n = scheduler_.NumberOfThreads();
  DB7_ASSERT(n >= 1, "Must have at least one thread");
  pipeline.SetNumTasks(n);
  std::vector<parallel::TaskQObj> batch;
  batch.resize(n);
  for (u32 i = 0; i < n; i++) { batch[i] = parallel::Task(&pipeline, &query_); }
  pipeline.SetState(RUNNING);
  scheduler_.ScheduleTasks(batch);
}

} // namespace db7::execution