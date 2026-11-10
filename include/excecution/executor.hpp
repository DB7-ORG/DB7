#pragma once

#include "excecution/parallel/task_scheduler.hpp"
#include "excecution/pipeline.hpp"
#include "excecution/query_context.hpp"
#include "shared/macro_helper.hpp"

namespace db7::execution {
/** Non-owning, non-allocating completion callback. */
struct Completion {
  using Fn = void (*)(void *ctx);
  Fn fn = nullptr;
  void *ctx = nullptr;

  void operator()() const { fn(ctx); }
  explicit operator bool() const { return fn != nullptr; }

  /** Bind a member function: Completion::Bind<&Connection::OnQueryDone>(conn) */
  template <auto Method, typename T>
  static Completion Bind(T *obj) {
    return {[](void *p) { (static_cast<T *>(p)->*Method)(); }, obj};
  }
};

class Executor {
private:
  QueryContext &query_;
  parallel::TaskScheduler &scheduler_;
  std::vector<std::unique_ptr<Pipeline>> pipelines_;
  Completion on_done_;

private:
  void Finish() {
    Completion done = on_done_; // copy out first: the callback may destroy *this
    done();                     // must be the last thing touching this executor
  }

  bool SchedulePipeline() {
    for (auto &pipeline : pipelines_) {
      if (pipeline->Available()) {
        SchedulePipeline(*pipeline.get());
        return true;
      }
    }
    return false;
  }

  void SchedulePipeline(Pipeline &pipeline) {
    auto n = scheduler_.NumberOfThreads();
    DB7_ASSERT(n >= 1, "Must have atleast one thread");
    pipeline.SetNumTasks(n);
    std::vector<parallel::TaskQObj> batch;
    batch.resize(n);
    for (u32 i = 0; i < n; i++) { batch[i] = parallel::Task(&pipeline, &query_); }
    pipeline.SetState(RUNNING);
    scheduler_.ScheduleTasks(batch);
  }

public:
  Executor(QueryContext &q, parallel::TaskScheduler &s, Completion on_done,
           void *physical = nullptr)
      : query_(q), scheduler_(s), on_done_(on_done) {
    DB7_ASSERT(static_cast<bool>(on_done_), "Completion callback required");

    (void)physical;
    // TODO create pipelines from physical plan
  }

  void Start() {
    if (SchedulePipeline()) return;
    DB7_ASSERT(pipelines_.empty(), "Pipelines exist but none is runnable (cycle?)");
    Finish();
  }

  /** Called by pipeline that finished */
  void OnPipelineFinished() {
    if (SchedulePipeline()) return;
    DB7_ASSERT(std::all_of(pipelines_.begin(), pipelines_.end(),
                           [](auto &p) { return p->state_ == DONE; }),
               "No runnable pipeline but some never ran (cycle or bad DependsOn?)");
    Finish();
  }
};
} // namespace db7::execution