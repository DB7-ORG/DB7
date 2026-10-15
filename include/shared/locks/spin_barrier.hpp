
#include "common.hpp"

#include <atomic>
#include <emmintrin.h>

namespace db7::shared {
class SpinBarrier {
private:
  std::atomic<u64> counter_;

public:
  void Lock() { counter_++; }

  void Unlock() { counter_--; }

  /** TODO transaction Try()
   * One issue there: Try() waits for all in-flight commits, including ones with timestamps after
   * the new transaction’s start. Under a steady stream of commits the counter may rarely reach 0,
   * and BeginTransaction can stall for a long time.
   */
  void Try() {
    while (counter_.load() > 0) { _mm_pause(); }
  }

  class SpinBarrierGuard {
    SpinBarrier &lock_;

  public:
    SpinBarrierGuard(SpinBarrier &lock) : lock_(lock) { lock_.Lock(); }
    ~SpinBarrierGuard() { lock_.Unlock(); }
    SpinBarrierGuard(const SpinBarrierGuard &) = delete;
    SpinBarrierGuard &operator=(const SpinBarrierGuard &) = delete;
  };
};
} // namespace db7::shared