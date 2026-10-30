#pragma once

#include "common.hpp"
#include "storage/page.hpp"

namespace db7::storage {
enum AccessStrategyType { NORMAL, BULK_READ };

struct VictimPage {
  Page *page;
  PageIdentifier page_id;
  frame_idx_t frame_idx;

  VictimPage() : page(nullptr), page_id(0), frame_idx(0) {}

  VictimPage(Page *p, PageIdentifier id, frame_idx_t idx) : page(p), page_id(id), frame_idx(idx) {}
};

class AccessStrategy {
private:
  AccessStrategyType type_;
  frame_idx_t nbuffers_;
  frame_idx_t current_;
  std::vector<Page *> ring_;

private:
  u32 CalcRingSize(AccessStrategyType type);

  u32 Advance();

public:
  AccessStrategy() : type_(NORMAL) {}

  AccessStrategy(AccessStrategyType type);

  AccessStrategyType GetType() const { return type_; }
  bool IsDefaultType() const { return type_ == NORMAL; }

  VictimPage GetVictim(std::span<Page> pages_, PageIdentifier id, bool isIO);

  bool IsFull() { return ring_.size() == nbuffers_; }
  void Append(Page *page) { ring_.push_back(page); }
  void Replace(Page *page) {
    DB7_ASSERT(IsFull(), "Upsert called on non filled ring_");
    ring_[Advance()] = page;
  }
};

class ClockSweep {
private:
  std::atomic<u32> sweep_head_;

  /**
   * Finds a victim page to evict.
   *
   * @param pages_ main buffer pool buffer that stores pages
   * @param id     id of a new page that needs to take its place
   * @param isIO   should it set IO_IN_PROGRESS flag
   */
  VictimPage GetVictim(std::span<Page> pages_, PageIdentifier id, bool isIO);

public:
  ClockSweep() = default;

  /**
   * Finds a victim page to evict considering an access strategy policy.
   *
   * @param pages_ main buffer pool buffer that stores pages
   * @param id     id of a new page that needs to take its place
   * @param isIO   should it set IO_IN_PROGRESS flag
   */
  VictimPage GetVictim(std::span<Page> pages_, PageIdentifier id, AccessStrategy &strategy,
                       bool isIO);
};
} // namespace db7::storage