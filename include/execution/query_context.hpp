#pragma once

#include "common.hpp"

#include <atomic>
#include <limits>

namespace db7::execution {
class QueryContext {
private:
  u64 query_id;
  u32 max_threads{std::numeric_limits<u32>::max()};
  std::atomic<bool> cancelled{false};
  void *error;

public:
  void Cancel() { cancelled.store(true); }
  bool IsCancelled() { return cancelled; }
};
} // namespace db7::execution