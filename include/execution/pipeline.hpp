#pragma once

#include "execution/parallel/exec_task.hpp"
#include "execution/piepeline_state.hpp"

#include <memory>
#include <span>

namespace db7::execution {
class Executor;

enum PipelineState : u8 { WAITING, RUNNING, DONE };

class Pipeline {
private:
  Executor &executor_;
  std::unique_ptr<PipelineContext> context_;
  std::vector<Pipeline *> parents_;
  std::atomic<u32> deps_remaining_{0};  // parents not yet finished
  std::atomic<u32> tasks_remaining_{0}; // tasks of this pipeline still running
  PipelineState state_;
  std::unique_ptr<parallel::Task[]> tasks_;

public:
  Pipeline(Executor &executor, std::unique_ptr<PipelineContext> state);
  ~Pipeline() = default;
  Pipeline(const Pipeline &) = delete;
  Pipeline &operator=(const Pipeline &) = delete;

  PipelineContext &State();

  void DependsOn(Pipeline &child);

  void TaskDone() noexcept;
  void PipelineDone() noexcept;

private:
  friend class Executor;
  std::span<parallel::Task> CreateTasks(u32 n, QueryContext &q, parallel::TaskScheduler &s);
  void SetNumTasks(u32 n);
  void SetState(PipelineState state);
  bool Available() const;
};
} // namespace db7::execution