#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/fsm/fsm_index.hpp"

namespace db7::storage {
BufferPool::BufferPool(DiskScheduler *disk_mng, PageVersionManager *version_table,
                       frame_idx_t page_num)
    : disk_mng_(disk_mng), pool_size_(page_num), version_table_(version_table) {
  pages_ = new Page[page_num];
  auto block_size = static_cast<frame_idx_t>(page_num) * DB7_PAGE_SIZE;
  auto data = static_cast<u8 *>(std::aligned_alloc(4096, block_size));
  DB7_ASSERT(data != nullptr, "Failed to allocate");
  PageIdentifier id(INVALID_PAGE_IDENTFIER);
  for (frame_idx_t i = 0; i < page_num; i++) {
    pages_[i].SetId(id);
    pages_[i].SetData(data + (i * DB7_PAGE_SIZE));
  }

  size_t per_partition = page_num / BUFFER_POOL_PARTITION_NUM;
  for (u32 i = 0; i < BUFFER_POOL_PARTITION_NUM; i++) { partitions_.Reserve(per_partition, i); }

  FreeSpaceManagerIndex::Reset();
  disk_mng_->io_->RegisterPool(pages_[0].GetData(), block_size);
}

BufferPool::~BufferPool() {
  std::free(pages_[0].GetData());
  delete[] pages_;
}
/** Noone should be able to modify this page flags since Tag is invalid */
void BufferPool::UndoState(VictimPage victim) {
  auto *victim_page = victim.page;
  victim_page->SetId(victim.page_id);

  flags_t state = victim_page->State();
  flags_t next;
  do {
    next = Page::SetTagValid(Page::Unpin(Page::ClearIOInProgress(state)));
    if (victim_page->CasState(state, next)) { break; }
  } while (true);

  if (!Page::IsPinned(next)) { replacement_.NotifyFrameFree(); }
  victim_page->NotifyIO();
}

bool BufferPool::PageVisit(Page *page, PageIdentifier id, flags_t state) noexcept {
  if (page->GetId() == id) {
    page->IncUsageCount(state);
    return true;
  }
  return false;
}

template <typename ReadFn>
BufferPool::PinResult BufferPool::PinForBlock(PageIdentifier id, PinOptions &opts, ReadFn read_fn) {
  DB7_ASSERT(id.pid != 0, "invalid pid");
  DB7_ASSERT(id.pid != std::numeric_limits<page_id>::max(), "invalid pid");

  u32 part_idx = partitions_.GetPartitionIdx(id);

  while (true) {
    // this function pins the frame because of the instane 5 thread race :D
    flags_t state;
    frame_idx_t frame_idx = partitions_.GetAndApply(
        id, part_idx, [this](frame_idx_t f) -> flags_t { return pages_[f].Pin(); }, state);
    if (partitions_.IsValid(frame_idx)) {
      Page *page = &pages_[frame_idx];
      if (Page::IsTagValid(state) && PageVisit(page, id, state)) { return {page, PinKind::Hit}; }
      Unpin(page);
    }

    VictimPage victim = GetVictim(id, opts.strategy, true);

    frame_idx_t new_frame_idx;
    if (partitions_.PutAndApply(
            id, victim.frame_idx, new_frame_idx, part_idx,
            [this](frame_idx_t f) { pages_[f].SetTagValid(); },
            [this](frame_idx_t f) -> flags_t { return pages_[f].Pin(); }, state)) {

      if (victim.page_id != INVALID_PAGE_IDENTFIER) {
        partitions_.Delete(victim.page_id, victim.frame_idx);
      }

      if (opts.pkind == PageKind::Heap) {
        victim.page->SetVersions(version_table_->Get(id));
      } else {
        victim.page->SetVersions(nullptr);
      }

      if (!read_fn(victim.page, id)) {
        // UndoState(victim);
        // partitions_.Delete(id, victim.frame_idx, part_idx);
        FailRead(victim.page, id);
        Unpin(victim.page);
        throw MEMORY_EXCEPTION("Failure inside disk scheduler");
      }

      // caller must submit the read
      return {victim.page, PinKind::Miss};
    }

    UndoState(victim);

    Page *page = &pages_[new_frame_idx];
    if (Page::IsTagValid(state) && PageVisit(page, id, state)) { return {page, PinKind::Foreign}; }
    Unpin(page);

    stats_.Add(PinKind::SpinMiss);
    continue;
  }
}

void BufferPool::FailRead(Page *page, PageIdentifier id) {
  partitions_.Delete(id, static_cast<frame_idx_t>(page - pages_)); // first, so no new pinners

  flags_t state = page->State();
  flags_t next;
  do {
    next = Page::ClearTagValid(Page::ClearIOInProgress(state));
  } while (!page->CasState(state, next));

  page->NotifyIO();
}

Page *BufferPool::Pin(PageIdentifier id, PageKind pkind) {
  AccessStrategy strategy;
  PinOptions opts(pkind, strategy);
  return Pin(id, opts);
}

Page *BufferPool::Pin(PageIdentifier id, PinOptions &opts) {
  PinResult pres = PinForBlock(id, opts, [this](Page *page, PageIdentifier pid) {
    return disk_mng_->Enqueue(IoTask(IoTask::READ, IoPriority::HIGH, page, pid));
  });
  stats_.Add(pres.kind);
  return pres.page;
}

PinRangeResult BufferPool::PinRange(PageRange range, Page **out, PageKind pkind) {
  AccessStrategy strategy;
  PinOptions opts(pkind, strategy);
  return PinRange(range, out, opts);
}

// Pins up to n consecutive pages starting at first_pid into out[].
// Stops after the first hit or foreign page, which is included in out[].
// out[0 .. io_count) are misses, submitted together as one vectored read.
// Returns the number of pages pinned.
PinRangeResult BufferPool::PinRange(PageRange range, Page **out, PinOptions &opts) {
  DB7_ASSERT(range.size <= IO_MAX_COMBINE, "range too large");
  u32 count = 0;
  u32 misses = 0;
  auto id_at = [&](u32 i) { return PageIdentifier(range.first.tbl_id, range.first.pid + i); };

  try {
    while (count < range.size) {
      PinResult res = PinForBlock(id_at(count), opts);
      out[count] = std::move(res.page);
      count++;
      if (res.kind != PinKind::Miss) {
        stats_.Add(res.kind);
        break; // cached or being loaded: ends the run, but it's pinned, so return it
      }
      misses++;
    }

    if (misses > 0) {
      stats_.Add(PinKind::Miss, misses);
      if (!disk_mng_->Enqueue(IoTask(IoTask::READV, IoPriority::HIGH, out, misses, range.first))) {
        throw MEMORY_EXCEPTION("Failed to enqueue a task to camel queue");
      }
    }
  } catch (...) {
    for (u32 i = 0; i < misses; ++i) { FailRead(out[i], id_at(i)); }
    for (u32 i = 0; i < count; ++i) { out[i]->Unpin(); }
    throw;
  }

  return {count, misses > 0};
}

void BufferPool::Unpin(Page *page, bool dirty) {
  if (dirty) { page->SetDirty(); }
  Unpin(page);
}

void BufferPool::Unpin(Page *page) {
  auto prev_state = page->Unpin();
  if (Page::PinCount(prev_state) == 1) { replacement_.NotifyFrameFree(); }
}

Page *BufferPool::Reserve(table_id tbl_id, AccessStrategy &strategy) {
  page_id pid = FreeSpaceManagerIndex::Get(tbl_id);

  auto id = PageIdentifier(tbl_id, pid);
  VictimPage victim = GetVictim(id, strategy, false);

  if (victim.page_id != INVALID_PAGE_IDENTFIER) {
    partitions_.Delete(victim.page_id, victim.frame_idx);
    victim.page->SetVersions(nullptr);
  }
  std::memset(victim.page->GetData(), 0, DB7_PAGE_SIZE); // TODO i dont needd this

  auto part_idx = partitions_.GetPartitionIdx(id);
  partitions_.SetAndApply(id, victim.frame_idx, part_idx,
                          [this](frame_idx_t f) { pages_[f].SetTagValid(); });
  return victim.page;
}

VictimPage BufferPool::GetVictim(PageIdentifier id, AccessStrategy &strategy, bool isIO) {
  auto pages_span = GetPages();
  // if (strategy.IsDefaultType()) { return replacement_.GetVictim(pages_span, id, isIO); }
  while (true) {
    auto reused = strategy.TryReuse(pages_span, id, isIO);
    if (reused.needs_flush) {
      if (!SafeFlush(reused.page)) { throw MEMORY_EXCEPTION("Disk scheduler err"); }
      continue;
    } else if (reused.IsValid()) {
      return reused;
    }

    auto victim = replacement_.GetVictim(pages_span, id, isIO);
    // DB7_ASSERT(victim.needs_flush == false, "Clock doesnt use flushing");
    if (victim.needs_flush) {
      if (!SafeFlush(victim.page)) { throw MEMORY_EXCEPTION("Disk scheduler err"); }
      continue;
    }
    if (!victim.IsValid()) {
      throw EXECUTION_EXCEPTION("Thread timed out", shared::ErrorCode::ERRCODE_NO_DATA);
    }
    strategy.Track(victim.page);
    return victim;
  }
}

bool BufferPool::SafeFlush(Page *page) noexcept {
  if (disk_mng_->Enqueue(IoTask(IoTask::WRITE, IoPriority::HIGH, page, page->GetId()))) {
    return true;
  }

  // undo path
  flags_t state = page->State();
  do {
    auto next = Page::ClearWriteInProgress(Page::SetDirty(state));
    if (page->CasState(state, next)) { break; }
  } while (true);

  page->NotifyIO();

  return false;
}

} // namespace db7::storage