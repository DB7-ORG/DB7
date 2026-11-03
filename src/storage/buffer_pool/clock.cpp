#include "storage/buffer_pool/clock.hpp"

namespace db7::storage {

AccessStrategy::AccessStrategy(AccessStrategyType type) : type_(type) {

  if (!IsDefaultType()) {
    DB7_ASSERT(std::has_single_bit(CalcRingSize(type)), "Musto be power of 2");
    ring_.emplace(CalcRingSize(type));
  }
}

VictimPage AccessStrategy::TryReuse(std::span<Page> pages, PageIdentifier id, bool isIO) {
  if (!ring_ || !ring_->Full()) { return {}; }

  Page *page = ring_->ReadUnsafe();
  if (!page->TryWLock()) { return {}; }

  if (page->IsPinned() || page->UsageCount() > 0 || page->IsIOInProgress() ||
      page->IsWriteInProgress()) {
    page->WUnlock();
    return {};
  }

  if (page->IsDirty()) {
    if (RejectDirty()) {
      page->WUnlock();
      return {};
    }
    page->ClearDirty();
    page->SetWriteInProgress();
    page->WUnlock();

    VictimPage result;
    result.page = page;
    result.frame_idx = static_cast<frame_idx_t>(page - pages.data());
    result.needs_flush = true;
    return result;
  }

  VictimPage result;
  result.page_id = page->GetId();
  page->SetId(id);
  page->Pin();
  if (isIO) { page->SetIOInProgress(); }
  page->WUnlock();

  result.frame_idx = static_cast<frame_idx_t>(page - pages.data());
  result.page = page;
  ring_->Append(page);
  return result;
}

VictimPage ClockSweep::GetVictim(std::span<Page> pages_, PageIdentifier id, bool isIO) {
  u32 max_iters = pages_.size() * 2;

  VictimPage result;
  for (u32 iters = 1; true; iters++) {
    frame_idx_t head = (sweep_head_++) % pages_.size();
    Page *page = &pages_[head];
    if (page->TryWLock()) {
      if (!page->IsPinned() && !page->IsWriteInProgress() && !page->DecUsageCount() &&
          !page->IsIOInProgress() && !page->IsDirty()) {
        result.page_id = page->GetId();
        page->SetId(id);
        page->Pin();
        if (isIO) { page->SetIOInProgress(); }
        page->WUnlock();
        result.frame_idx = head;
        result.page = page;
        return result;
      }
      page->WUnlock();
    }

    if (iters % max_iters == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
  }

  DB7_UNREACHABLE();
}
} // namespace db7::storage