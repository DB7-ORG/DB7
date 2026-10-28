#pragma once

#include "common.hpp"
#include "storage/page.hpp"

namespace db7::storage {
enum AccessStrategyType { NORMAL, BULK_READ };

class AccessStrategy {
private:
  AccessStrategyType type_;
  u32 nbuffers_;
  u32 current_;
  std::vector<Page *> ring_;

private:
  u32 CalcRingSize(AccessStrategyType type);

  u32 Advance();

public:
  AccessStrategy() : type_(NORMAL) {}

  AccessStrategy(AccessStrategyType type);

  AccessStrategyType GetType() const { return type_; }
  bool IsDefaultType() const { return type_ == NORMAL; }

  Page *GetVictim(std::span<Page> pages_, PageIdentifier id, u32 &victim_frame_idx,
                  PageIdentifier &victim_page_id, bool isIO);

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

public:
  ClockSweep() = default;

  Page *GetVictim(std::span<Page> pages_, PageIdentifier id, u32 &victim_frame_idx,
                  PageIdentifier &victim_page_id, bool isIO);
};
} // namespace db7::storage