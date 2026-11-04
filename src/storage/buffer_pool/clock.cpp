#include "storage/buffer_pool/clock.hpp"
#include "shared/helper.hpp"

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

VictimPage ClockSweep::Sweep(std::span<Page> pages, PageIdentifier id, bool isIO) {
  const u32 n = pages.size();

  u32 misses = 0;
  Defer miss_stats([&] { stats_.Add(EvictionType::MISS, misses); });

  for (; misses < n;) {
    frame_idx_t head = (sweep_head_++) % n;
    Page *page = &pages[head];

    if (!page->TryWLock()) {
      misses++;
      continue;
    }

    if (page->IsPinned() || page->IsWriteInProgress() || page->IsIOInProgress()) {
      page->WUnlock();
      misses++;
      continue;
    }
    if (page->UsageCount() > 0) {
      page->DecUsageCount();
      page->WUnlock();
      stats_.Add(EvictionType::MISS, misses);
      misses = 0; // progress
      continue;
    }
    if (page->IsDirty()) {
      page->ClearDirty();
      page->SetWriteInProgress();
      page->WUnlock();

      stats_.Add(EvictionType::WRITTEN);
      VictimPage result;
      result.page = page;      // missing
      result.frame_idx = head; // missing
      result.needs_flush = true;
      return result;
    }

    auto page_id = page->GetId();
    page->SetId(id);
    page->Pin();
    if (isIO) { page->SetIOInProgress(); }
    page->WUnlock();

    stats_.Add(EvictionType::HIT);
    return {page, page_id, head};
  }

  return {};
}

VictimPage ClockSweep::GetVictim(std::span<Page> pages, PageIdentifier id, bool isIO) {
  const auto deadline = Clock::now() + kVictimTimeout;
  for (;;) {
    auto v = Sweep(pages, id, isIO);
    if (v.IsValid()) { return v; }

    // Register, then rescan, so a frame freed before we sleep isn't missed
    waiters_.fetch_add(1, std::memory_order_seq_cst);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    u64 seen = wake_epoch_.load();

    v = Sweep(pages, id, isIO);
    if (!v.IsValid()) {
      std::unique_lock lk(wait_mu_);
      wait_cv_.wait_for(lk, threadWaitFor, [&] { return wake_epoch_.load() != seen; });
    }
    waiters_.fetch_sub(1, std::memory_order_seq_cst);

    if (v.IsValid()) { return v; }
    if (Clock::now() >= deadline) return {}; // caller aborts and drops its pins
  }
}

void ClockSweep::NotifyFrameFree() {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  if (waiters_.load(std::memory_order_seq_cst) == 0) return; // fast path
  {
    std::lock_guard g(wait_mu_);
    wake_epoch_.fetch_add(1);
  }
  wait_cv_.notify_one();
}
} // namespace db7::storage