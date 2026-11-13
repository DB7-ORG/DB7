#pragma once

#include <atomic>

namespace db7::shared {
struct Waiter {
  Waiter *next = nullptr;
  void (*wake_fn)(Waiter *) = nullptr; // task: re-enqueue; thread: unblock
  void Wake() { wake_fn(this); }
};

class WaitList {
private:
  static inline Waiter *const kSignaled = reinterpret_cast<Waiter *>(uintptr_t{1});
  std::atomic<Waiter *> head_{kSignaled};

public:
  // The operation owner calls this BEFORE publishing "in progress".
  void Reset() { head_.store(nullptr, std::memory_order_relaxed); }

  // Returns false if already signaled: don't park, just continue.
  bool TryPark(Waiter *w) {
    Waiter *h = head_.load(std::memory_order_acquire);
    do {
      if (h == kSignaled) return false;
      w->next = h;
    } while (
        !head_.compare_exchange_weak(h, w, std::memory_order_release, std::memory_order_acquire));
    return true;
  }

  void Signal() {
    Waiter *w = head_.exchange(kSignaled, std::memory_order_acq_rel);
    while (w) {
      Waiter *next = w->next; // read BEFORE Wake: the waker relinks w into the ready queue
      w->Wake();
      w = next;
    }
  }
};

} // namespace db7::shared