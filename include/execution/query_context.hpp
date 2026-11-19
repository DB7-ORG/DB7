#pragma once

#include "common.hpp"
#include "shared/error/error_data.hpp"

#include <atomic>
#include <limits>

namespace db7::execution {
class QueryContext {
private:
  u64 query_id;
  u32 max_threads{std::numeric_limits<u32>::max()};
  std::atomic<bool> cancelled{false};
  std::atomic<shared::ErrorData *> error_{nullptr};

public:
  QueryContext() {
    (void)query_id;
    (void)max_threads;
  }
  ~QueryContext() { delete error_.load(std::memory_order_relaxed); }

  void Cancel() { cancelled.store(true); }
  bool IsCancelled() { return cancelled; }

  void SetError(shared::ErrorData e) {
    auto *mine = new shared::ErrorData(std::move(e));
    shared::ErrorData *expected = nullptr;
    if (!error_.compare_exchange_strong(expected, mine, std::memory_order_release,
                                        std::memory_order_relaxed)) {
      delete mine; // another task's error came first
    }
    Cancel();
  }

  /** nullptr if the query didn't fail. */
  const shared::ErrorData *Error() const { return error_.load(std::memory_order_acquire); }
};
} // namespace db7::execution