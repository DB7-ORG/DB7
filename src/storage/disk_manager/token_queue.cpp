#include "storage/disk_manager/token_queue.hpp"

#include <atomic>

namespace db7::shared::detail {

constinit thread_local TokenCache tls_token_cache{};

u64 NextQueueId() {
  static std::atomic<u64> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}

void InsertToken(u64 owner, void *token) {
  TokenCache &c = tls_token_cache;
  c.e[c.next_victim++ % kTokenCacheSlots] = {owner, token};
}

} // namespace db7::shared::detail