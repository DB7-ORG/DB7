#include "excecution/parallel/task_scheduler_pool.hpp"
#include "excecution/parallel/task_scheduler.hpp"
#include "shared/config/config.hpp"
#include "shared/macro_helper.hpp"
#include "third_party/lightweightsemaphore.h"

#include <pthread.h>
#include <sched.h>

namespace db7::execution::parallel {

static constexpr u32 kMaxSchedulerThreads = 1024;

// CPUs this process may run on (respects taskset, cpusets, numactl). Empty if unknown.
static std::vector<int> GetProcessCPUMask() {
#if defined(__GLIBC__)
  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) != 0) return {};
  std::vector<int> cpus;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, &set)) cpus.push_back(cpu);
  }
  return cpus;
#else
  return {};
#endif
}

// Best effort: if it fails, the thread simply runs unpinned.
static void SetThreadAffinity(std::thread &t, int cpu) {
#if defined(__GLIBC__)
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  pthread_setaffinity_np(t.native_handle(), sizeof(set), &set);
#else
  (void)t;
  (void)cpu;
#endif
}

static u32 DefaultThreadCount() {
  auto cpus = GetProcessCPUMask();
  if (!cpus.empty()) return u32(cpus.size());
  unsigned n = std::thread::hardware_concurrency();
  return n == 0 ? 1 : n;
}

struct LightWeightSemaphoreWrapper : moodycamel::LightweightSemaphore {
  using moodycamel::LightweightSemaphore::LightweightSemaphore; // inherit constructors
};

TaskSchedulerPool::TaskSchedulerPool()
    : requested_thread_count_(0), current_thread_count_(0),
      semaphore_(std::make_unique<LightWeightSemaphoreWrapper>()) {
  SetThreads(DbConfig::Get(TASK_SCHEDULER_THREAD_COUNT));
  thread_pin_ = DbConfig::Get(TASK_SCHEDULER_PINNED_THREADS);
}

TaskSchedulerPool::~TaskSchedulerPool() { Stop(); }

void TaskSchedulerPool::SetThreads(idx_t n) {
  DB7_ASSERT(n <= kMaxSchedulerThreads && "TASK_SCHEDULER_THREAD_COUNT too large");
  u32 count = n == 0 ? DefaultThreadCount() : u32(std::min<idx_t>(n, kMaxSchedulerThreads));
  requested_thread_count_.store(count, std::memory_order_relaxed);
}

u32 TaskSchedulerPool::NumberOfThreads() const { return current_thread_count_.load(); }

void TaskSchedulerPool::Signal(idx_t n) {
  if (n == 0) { return; }
  semaphore_->signal(n);
}

void TaskSchedulerPool::Wait() { semaphore_->wait(); }

bool TaskSchedulerPool::Wait(int64_t timeout_usecs) { return semaphore_->wait(timeout_usecs); }

void TaskSchedulerPool::Run(TaskScheduler &scheduler, std::atomic<bool> *marker) {
  scheduler.ExecuteForever(marker);
}

void TaskSchedulerPool::Stop() {
  for (auto &m : markers_) m->store(false, std::memory_order_relaxed);
  semaphore_->signal(int(threads_.size()));
  for (auto &t : threads_) t->join();
  threads_.clear();
  markers_.clear();
  current_thread_count_.store(0, std::memory_order_relaxed);
}

void TaskSchedulerPool::Start(TaskScheduler &scheduler) {
  u32 n = requested_thread_count_.load(std::memory_order_relaxed);
  auto cpus = thread_pin_ ? GetProcessCPUMask() : std::vector<int>{};
  threads_.reserve(n);
  markers_.reserve(n);
  for (u32 i = 0; i < n; i++) {
    auto marker = std::make_unique<std::atomic<bool>>(true);
    std::atomic<bool> *m = marker.get();
    markers_.push_back(std::move(marker));
    threads_.push_back(std::make_unique<std::thread>([this, &scheduler, m] { Run(scheduler, m); }));
    if (i < cpus.size()) SetThreadAffinity(*threads_.back(), cpus[i]);
  }
  current_thread_count_.store(n, std::memory_order_relaxed);
}

void TaskSchedulerPool::Reload(TaskScheduler &scheduler) {
  SetThreads(DbConfig::Get(TASK_SCHEDULER_THREAD_COUNT));
  auto new_thread_pin_ = DbConfig::Get(TASK_SCHEDULER_PINNED_THREADS);
  if (current_thread_count_ == requested_thread_count_ && new_thread_pin_ == thread_pin_) {
    return;
  }
  thread_pin_ = new_thread_pin_;
  Stop();
  Start(scheduler);
}

} // namespace db7::execution::parallel