#include "shared/data_structures/token_queue.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace db7::shared::detail {

constinit thread_local TokenCache tls_token_cache{};

static std::atomic<u32> g_next_id{0};

u32 NextQueueId() {
  u32 id = g_next_id.fetch_add(1, std::memory_order_relaxed);
  if (id >= kMaxTokenQueues) {
    std::fprintf(stderr, "TokenQueue: more than %u queues created\n", kMaxTokenQueues);
    std::abort();
  }
  return id;
}

} // namespace db7::shared::detail