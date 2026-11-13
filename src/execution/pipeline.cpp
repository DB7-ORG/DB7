#include "execution/pipeline.hpp"
#include "execution/executor.hpp"
#include "shared/macro_helper.hpp"

namespace db7::execution {

Pipeline::Pipeline(Executor &executor, std::unique_ptr<PipelineContext> state)
    : executor_(executor), context_(std::move(state)), state_(PipelineState::WAITING) {}

PipelineContext &Pipeline::State() { return *context_; }

void Pipeline::DependsOn(Pipeline &child) {
  child.parents_.push_back(this);
  deps_remaining_.fetch_add(1, std::memory_order_relaxed);
}

void Pipeline::TaskDone() noexcept {
  if (tasks_remaining_.fetch_sub(1) != 1) { return; }

  PipelineDone();

  executor_.OnPipelineFinished();
}

void Pipeline::PipelineDone() noexcept {
  DB7_ASSERT(tasks_remaining_ == 0, "All tasks are finished");
  state_ = DONE;
  for (auto &parent : parents_) { parent->deps_remaining_.fetch_sub(1); }
}

void Pipeline::SetNumTasks(u32 n) { tasks_remaining_.store(n); }
void Pipeline::SetState(PipelineState state) { state_ = state; }
bool Pipeline::Available() const {
  return state_ == PipelineState::WAITING && deps_remaining_.load() == 0;
}

std::span<parallel::Task> Pipeline::CreateTasks(u32 n, QueryContext &q,
                                                parallel::TaskScheduler &s) {
  tasks_ = std::make_unique<parallel::Task[]>(n); // 1 allocation
  for (u32 i = 0; i < n; i++) { tasks_[i].Init(this, &q, &s); }
  tasks_remaining_.store(n);
  return {tasks_.get(), n};
}
} // namespace db7::execution