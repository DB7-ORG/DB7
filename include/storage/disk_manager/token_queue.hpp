#pragma once

#include "common.hpp"
#include "third_party/concurrentqueue_safe.hpp" // IWYU pragma: keep

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace db7::shared {

namespace detail {

// One small per-thread cache shared by every TokenQueue instance (of any T).
// Maps a queue's unique id to this thread's ProducerToken for that queue.
inline constexpr u32 kTokenCacheSlots = 8;

struct TokenCache {
  struct Entry {
    u64 owner; // 0 = empty; queue ids start at 1 and are never reused
    void *token;
  };
  Entry e[kTokenCacheSlots];
  u32 next_victim;
};

// constinit lets the compiler access this directly, without a TLS init wrapper call.
extern constinit thread_local TokenCache tls_token_cache;

u64 NextQueueId();
void InsertToken(u64 owner, void *token); // slow path, round-robin replacement

inline void *LookupToken(u64 owner) {
  for (auto &e : tls_token_cache.e)
    if (e.owner == owner) return e.token;
  return nullptr;
}

} // namespace detail

// moodycamel::ConcurrentQueue that always enqueues through a per-thread ProducerToken.
// Tokens are created lazily on a thread's first Enqueue (or by RegisterThread) and are
// owned by the queue, so they never outlive it, even if threads outlive the queue.
template <typename T, typename Traits = moodycamel::ConcurrentQueueDefaultTraits>
class TokenQueue {
public:
  using Queue = moodycamel::ConcurrentQueue<T, Traits>;

  TokenQueue() = default;
  explicit TokenQueue(size_t initial_capacity) : queue_(initial_capacity) {}

  TokenQueue(const TokenQueue &) = delete;
  TokenQueue &operator=(const TokenQueue &) = delete;

  // Optional warm-up so the first Enqueue on this thread skips the slow path.
  void RegisterThread() { (void)Token(); }

  // ---- producer side (always tokened) ----
  bool Enqueue(T &&item) { return queue_.enqueue(Token(), std::move(item)); }
  bool Enqueue(const T &item) { return queue_.enqueue(Token(), item); }

  template <typename It>
  bool EnqueueBulk(It first, size_t count) {
    return queue_.enqueue_bulk(Token(), first, count);
  }

  // ---- consumer side (plain passthrough) ----
  bool TryDequeue(T &out) { return queue_.try_dequeue(out); }

  template <typename It>
  size_t TryDequeueBulk(It first, size_t max) {
    return queue_.try_dequeue_bulk(first, max);
  }

  size_t SizeApprox() const { return queue_.size_approx(); }

private:
  moodycamel::ProducerToken &Token() {
    if (void *t = detail::LookupToken(id_)) [[likely]]
      return *static_cast<moodycamel::ProducerToken *>(t);
    return RegisterSlow();
  }

  moodycamel::ProducerToken &RegisterSlow() {
    moodycamel::ProducerToken *tok;
    {
      std::lock_guard lk(tokens_mu_);
      auto &slot = tokens_[std::this_thread::get_id()];
      if (!slot) slot = std::make_unique<moodycamel::ProducerToken>(queue_);
      tok = slot.get();
    }
    detail::InsertToken(id_, tok);
    return *tok;
  }

  const u64 id_ = detail::NextQueueId();
  Queue queue_;

  // Declared after queue_ so tokens are destroyed before the queue they point into.
  // Keyed by thread id: a new thread that reuses a dead thread's id takes over its token.
  std::mutex tokens_mu_;
  std::unordered_map<std::thread::id, std::unique_ptr<moodycamel::ProducerToken>> tokens_;
};

} // namespace db7::shared