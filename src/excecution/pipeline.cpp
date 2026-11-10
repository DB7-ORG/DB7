#include "excecution/pipeline.hpp"
#include "excecution/executor.hpp"
#include "shared/macro_helper.hpp"

namespace db7::execution {

Pipeline::Pipeline(Executor &executor, std::unique_ptr<PipelineContext> state)
    : executor_(executor), context_(std::move(state)), state_(PipelineState::WAITING) {}

PipelineContext &Pipeline::State() { return *context_; }

void Pipeline::DependsOn(Pipeline &child) {
  child.parents_.push_back(this);
  deps_remaining_.fetch_add(1, std::memory_order_relaxed);
}

void Pipeline::TaskDone() {
  if (tasks_remaining_.fetch_sub(1) != 1) { return; }

  PipelineDone();

  executor_.OnPipelineFinished();
}

void Pipeline::PipelineDone() {
  DB7_ASSERT(tasks_remaining_ == 0, "All tasks are finished");
  state_ = DONE;
  for (auto &parent : parents_) { parent->deps_remaining_.fetch_sub(1); }
}
} // namespace db7::execution