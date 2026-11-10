#pragma once

#include "common.hpp"
#include "third_party/concurrentqueue_safe.hpp" // IWYU pragma: keep

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace db7::shared {

namespace detail {

// Per-thread table of ProducerTokens, indexed directly by queue id.
// Shared by every TokenQueue instance (of any T). Ids are 0..kMaxTokenQueues-1, never reused.
inline constexpr u32 kMaxTokenQueues = 16;

struct TokenCache {
  moodycamel::ProducerToken *token[kMaxTokenQueues]; // nullptr = not registered yet
};

// constinit lets the compiler access this directly, without a TLS init wrapper call.
extern constinit thread_local TokenCache tls_token_cache;

u32 NextQueueId();

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
    if (auto *t = detail::tls_token_cache.token[id_]) [[likely]]
      return *t;
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
    detail::tls_token_cache.token[id_] = tok;
    return *tok;
  }

  const u32 id_ = detail::NextQueueId();
  Queue queue_;

  // Declared after queue_ so tokens are destroyed before the queue they point into.
  // Keyed by thread id: a new thread that reuses a dead thread's id takes over its token.
  std::mutex tokens_mu_;
  std::unordered_map<std::thread::id, std::unique_ptr<moodycamel::ProducerToken>> tokens_;
};

} // namespace db7::shared