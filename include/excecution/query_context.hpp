#pragma once

#include "common.hpp"

#include <atomic>
#include <limits>

namespace db7::execution {
struct QueryContext {
  u64 query_id;
  u32 max_threads{std::numeric_limits<u32>::max()};
  std::atomic<bool> cancelled{false};
};
} // namespace db7::execution