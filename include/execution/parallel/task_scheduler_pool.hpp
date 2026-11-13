#pragma once

#include "common.hpp"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace db7::execution::parallel {

struct LightWeightSemaphoreWrapper;
class TaskScheduler;

class TaskSchedulerPool {
private:
  std::vector<std::unique_ptr<std::thread>> threads_;
  std::vector<std::unique_ptr<std::atomic<bool>>> markers_;
  std::atomic<u32> requested_thread_count_;
  std::atomic<u32> current_thread_count_;
  std::unique_ptr<LightWeightSemaphoreWrapper> semaphore_;
  bool thread_pin_;

  void Run(TaskScheduler &scheduler, std::atomic<bool> *marker);
  void Start(TaskScheduler &scheduler);
  void Stop();

public:
  TaskSchedulerPool();
  ~TaskSchedulerPool();

  void Signal(idx_t n);
  void Wait();
  bool Wait(int64_t timeout_usecs);

  u32 NumberOfThreads() const;
  void SetThreads(idx_t n);

  /** NOTE: NOT THREAD SAFE */
  void Reload(TaskScheduler &scheduler);
};
} // namespace db7::execution::parallel