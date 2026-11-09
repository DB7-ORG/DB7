#pragma once

#include "common.hpp"

#include <memory>
#include <thread>
#include <vector>

namespace db7::excecution::parallel {

struct LightWeightSemaphoreWrapper;

class TaskSchedulerPool {
private:
  std::vector<std::unique_ptr<std::thread>> threads_;
  std::vector<std::unique_ptr<std::atomic<bool>>> markers;
  std::atomic<u32> requested_thread_count;
  std::atomic<u32> current_thread_count;
  std::unique_ptr<LightWeightSemaphoreWrapper> semapthore_;

public:
  TaskSchedulerPool() = default;
  ~TaskSchedulerPool();
};
} // namespace db7::excecution::parallel