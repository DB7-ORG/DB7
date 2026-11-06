#pragma once

#include "common.hpp"
#include "shared/data_structures/ring_buffer.hpp"
#include "storage/buffer_pool/clock_monitoring.hpp"
#include "storage/page.hpp"

#include <mutex>
#include <optional>

namespace db7::storage {
enum AccessStrategyType { NORMAL, BULK_READ, BULK_WRITE };

struct VictimPage {
  Page *page;
  PageIdentifier page_id;
  frame_idx_t frame_idx;
  bool needs_flush;

  VictimPage() : page(nullptr), page_id(0), frame_idx(0), needs_flush(false) {}

  VictimPage(Page *p, PageIdentifier id, frame_idx_t idx)
      : page(p), page_id(id), frame_idx(idx), needs_flush(false) {}

  bool IsValid() const { return page != nullptr; }
};

class AccessStrategy {
private:
  AccessStrategyType type_;
  std::optional<shared::Ringbuffer<Page *>> ring_;

private:
  static constexpr u32 CalcRingSize(AccessStrategyType type) {
    switch (type) {
    case NORMAL: return 0;
    case BULK_READ: return 64;
    case BULK_WRITE: return 32;
    }
    DB7_UNREACHABLE();
  }

public:
  AccessStrategy() : type_(NORMAL) {}

  AccessStrategy(AccessStrategyType type);

  AccessStrategyType GetType() const { return type_; }
  bool IsDefaultType() const { return type_ == NORMAL; }
  u32 GetRingSize() const { return CalcRingSize(type_); }
  bool RejectDirty() const { return type_ == BULK_READ; }

  /** Adds a page chosen by the clock to the ring. */
  void Track(Page *page) {
    if (ring_) { ring_->Append(page); }
  }

  /**
   * Tries to reuse the oldest page in a full ring. On failure that page leaves
   * the ring, and the caller must Track() the replacement.
   */
  VictimPage TryReuse(std::span<Page> pages, PageIdentifier id, bool isIO);
};

class ClockSweep {
private:
  std::mutex sh_mutex_;
  alignas(CACHE_LINE_SIZE) std::atomic<frame_idx_t> sweep_head_;
  [[maybe_unused]] byte data_[CACHE_LINE_SIZE - sizeof(frame_idx_t)];
  std::atomic<u64> wake_epoch_{0};
  std::atomic<u32> waiters_{0};
  std::mutex wait_mu_;
  std::condition_variable wait_cv_;
  EvictionMonitor stats_;

private:
  using Clock = std::chrono::steady_clock;
  static constexpr auto kVictimTimeout = std::chrono::seconds(5);
  static constexpr auto threadWaitFor = std::chrono::milliseconds(10);

  frame_idx_t Tick(const u32 n);

  VictimPage Sweep(std::span<Page> pages, PageIdentifier id, bool isIO);

public:
  ClockSweep() = default;

  const EvictionMonitor &GetEvictionMonitor() const { return stats_; }

  frame_idx_t GetCurrent() const { return sweep_head_; }

  /**
   * Finds a victim page to evict.
   *
   * @param pages_ main buffer pool buffer that stores pages
   * @param id     id of a new page that needs to take its place
   * @param isIO   should it set IO_IN_PROGRESS flag
   */
  VictimPage GetVictim(std::span<Page> pages_, PageIdentifier id, bool isIO) noexcept;

  void NotifyFrameFree();
};
} // namespace db7::storage