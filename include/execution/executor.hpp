#pragma once

#include "execution/parallel/task_scheduler.hpp"
#include <memory>
#include <vector>

namespace db7::execution {
class Pipeline;
class QueryContext;
namespace parallel {
class TaskScheduler;
}

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
  QueryContext &query_; // TODO ownership make this uniqu ptr
  parallel::TaskScheduler &scheduler_;
  std::vector<std::unique_ptr<Pipeline>> pipelines_;
  Completion on_done_;

private:
  void Finish();
  bool SchedulePipeline();
  void SchedulePipeline(Pipeline &pipeline);

public:
  Executor(QueryContext &q, parallel::TaskScheduler &s, Completion on_done,
           void *physical = nullptr);
  ~Executor(); // defined in .cpp, where Pipeline is a complete type

  Executor(const Executor &) = delete;
  Executor &operator=(const Executor &) = delete;

  void Start();
  /** Called by the last task of a finished pipeline */
  void OnPipelineFinished() noexcept;
};
} // namespace db7::execution