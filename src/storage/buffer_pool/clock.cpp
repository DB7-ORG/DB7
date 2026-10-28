#include "storage/buffer_pool/clock.hpp"

namespace db7::storage {
u32 AccessStrategy::CalcRingSize(AccessStrategyType type) {
  switch (type) {
  case NORMAL: return 0;
  case BULK_READ:
    return 64;
    // case BULK_WRITE: return 256;
  }
  DB7_UNREACHABLE();
}

u32 AccessStrategy::Advance() { return (current_++) & (nbuffers_ - 1); }

AccessStrategy::AccessStrategy(AccessStrategyType type)
    : type_(type), nbuffers_(CalcRingSize(type)), current_(0) {
  if (!IsDefaultType()) {
    ring_.reserve(CalcRingSize(type));
    DB7_ASSERT(std::has_single_bit(CalcRingSize(type)), "Musto be power of 2");
  }
}

Page *AccessStrategy::GetVictim(std::span<Page> pages_, PageIdentifier id, u32 &victim_frame_idx,
                                PageIdentifier &victim_page_id, bool isIO) {
  for (u32 iters = 0; iters < nbuffers_; iters++) {
    u32 head = Advance();
    Page *page = ring_[head];
    if (page->TryWLock()) {
      if (!page->IsPinned() && page->UsageCount() <= 1 && !page->IsDirty() &&
          !page->IsIOInProgress()) {
        victim_page_id = page->GetId();
        page->SetId(id);
        page->Pin();
        if (isIO) { page->SetIOInProgress(); }
        page->WUnlock();
        victim_frame_idx = static_cast<u32>(page - pages_.data());
        return page;
      }
      page->WUnlock();
    }
  }

  return nullptr;
}

Page *ClockSweep::GetVictim(std::span<Page> pages_, PageIdentifier id, u32 &victim_frame_idx,
                            PageIdentifier &victim_page_id, bool isIO) {
  u32 max_iters = BUFFER_POOL_PAGE_NUM * 2;

  for (u32 iters = 1; true; iters++) {
    u32 head = (sweep_head_++) % pages_.size();
    Page *page = &pages_[head];
    if (page->TryWLock()) {
      if (!page->IsPinned() && !page->DecUsageCount() && !page->IsDirty() &&
          !page->IsIOInProgress()) {
        victim_page_id = page->GetId();
        page->SetId(id);
        page->Pin();
        if (isIO) { page->SetIOInProgress(); }
        page->WUnlock();
        victim_frame_idx = head;
        return page;
      }
      page->WUnlock();
    }

    if (iters % max_iters == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
  }

  DB7_UNREACHABLE();
}
} // namespace db7::storage