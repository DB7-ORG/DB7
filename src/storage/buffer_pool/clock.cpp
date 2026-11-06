#include "storage/buffer_pool/clock.hpp"
#include "shared/helper.hpp"
#include <atomic>

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
  flags_t state = page->State();
  while (true) {
    if (!page->IsReclaimable(state) || Page::UsageCount(state) > 0) { return {}; }

    if (Page::IsDirty(state)) {
      if (RejectDirty()) { return {}; }
      flags_t next = Page::SetWriteInProgress(Page::ClearDirty(state));
      if (!page->CasState(state, next)) { continue; }

      VictimPage result;
      result.page = page;
      result.frame_idx = static_cast<frame_idx_t>(page - pages.data());
      result.needs_flush = true;
      return result;
    }

    flags_t next = Page::ClearTagValid(Page::Pin(state));
    if (isIO) { next = Page::SetIOInProgress(next); }
    if (!page->CasState(state, next)) { continue; }

    VictimPage result;
    result.page_id = page->SwapId(id);
    result.frame_idx = static_cast<frame_idx_t>(page - pages.data());
    result.page = page;
    ring_->Append(page);
    return result;
  }
}

frame_idx_t ClockSweep::Tick(const u32 n) {
  u32 victim = sweep_head_.fetch_add(1);

  if (victim >= n) {
    const u32 original = victim;
    victim %= n;

    if (victim == 0) {
      u32 expected = original + 1;
      while (!sweep_head_.compare_exchange_weak(expected, expected % n)) {}
    }
  }
  return victim;
}

VictimPage ClockSweep::Sweep(std::span<Page> pages, PageIdentifier id, bool isIO) {
  const u32 n = pages.size();

  u32 misses = 0;
  Defer miss_stats([&] { stats_.Add(EvictionType::MISS, misses); });

  for (; misses < n;) {
    frame_idx_t head = Tick(n);
    Page *page = &pages[head];
    flags_t state = page->State();

    while (true) {
      if (!Page::IsReclaimable(state)) {
        misses++;
        break; // next frame
      }

      if (page->CheckDecUsageCount(state)) {
        stats_.Add(EvictionType::MISS, misses);
        misses = 0; // progress
        break;
      }

      if (Page::IsDirty(state)) {
        flags_t next = Page::SetWriteInProgress(Page::ClearDirty(state));
        if (!page->CasState(state, next)) continue;

        stats_.Add(EvictionType::WRITTEN);
        VictimPage result;
        result.page = page;
        result.frame_idx = head;
        result.needs_flush = true;
        return result;
      }

      // Pin, invalidate and start I/O in one atomic step
      flags_t next = Page::ClearTagValid(Page::Pin(state));
      if (isIO) next = Page::SetIOInProgress(next);
      if (!page->CasState(state, next)) continue;

      // The CAS won, so it's now safe to touch the id
      auto old_page_id = page->SwapId(id);

      stats_.Add(EvictionType::HIT);
      return {page, old_page_id, head};
    }
  }

  return {};
}

VictimPage ClockSweep::GetVictim(std::span<Page> pages, PageIdentifier id, bool isIO) {
  std::optional<Clock::time_point> deadline;

  for (;;) {
    auto v = Sweep(pages, id, isIO);
    if (v.IsValid()) { return v; }

    if (!deadline) {
      deadline = Clock::now() + kVictimTimeout;
    } else if (Clock::now() >= *deadline) {
      return {}; // caller aborts and drops its pins
    }

    waiters_.fetch_add(1, std::memory_order_relaxed);
    {
      std::unique_lock lk(wait_mu_);
      wait_cv_.wait_for(lk, threadWaitFor);
    }
    waiters_.fetch_sub(1, std::memory_order_relaxed);
  }
}

void ClockSweep::NotifyFrameFree() {
  if (waiters_.load(std::memory_order_relaxed) != 0) { wait_cv_.notify_one(); }
}
} // namespace db7::storage