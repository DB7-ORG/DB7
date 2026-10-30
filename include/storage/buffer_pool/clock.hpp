#pragma once

#include "common.hpp"
#include "storage/page.hpp"

namespace db7::storage {
enum AccessStrategyType { NORMAL, BULK_READ };

struct VictimPage {
  Page *page;
  PageIdentifier page_id;
  frame_idx_t frame_idx;
  bool needs_flush;

  VictimPage() : page(nullptr), page_id(0), frame_idx(0), needs_flush(false) {}

  VictimPage(Page *p, PageIdentifier id, frame_idx_t idx)
      : page(p), page_id(id), frame_idx(idx), needs_flush(false) {}
};

class AccessStrategy {
private:
  AccessStrategyType type_;
  frame_idx_t nbuffers_;
  frame_idx_t current_;
  frame_idx_t last_;
  std::vector<Page *> ring_;

private:
  u32 CalcRingSize(AccessStrategyType type) const;

  frame_idx_t Advance();

public:
  AccessStrategy() : type_(NORMAL) {}

  AccessStrategy(AccessStrategyType type);

  AccessStrategyType GetType() const { return type_; }
  bool IsDefaultType() const { return type_ == NORMAL; }

  VictimPage GetFromRing(std::span<Page> pages_, PageIdentifier id, bool isIO);

  bool RejectDirty();
  bool IsFull() { return ring_.size() == nbuffers_; }
  void Append(Page *page) { ring_.push_back(page); }
  void Remember(Page *page) {
    DB7_ASSERT(IsFull(), "Remember called on non filled ring_");
    ring_[last_] = page;
  }
  u32 GetRingSize() const { return CalcRingSize(type_); }
};

class ClockSweep {
private:
  std::atomic<frame_idx_t> sweep_head_;

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

  frame_idx_t GetCurrent() const { return sweep_head_; }
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