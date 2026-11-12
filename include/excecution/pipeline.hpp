#pragma once

#include "excecution/piepeline_state.hpp"

#include <memory>

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
  void SetNumTasks(u32 n);
  void SetState(PipelineState state);
  bool Available() const;
};
} // namespace db7::execution