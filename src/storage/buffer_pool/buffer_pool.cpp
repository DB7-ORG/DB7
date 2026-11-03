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
    pages_[i].WLock();
    pages_[i].SetId(id);
    pages_[i].WUnlock();
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

void BufferPool::UndoState(VictimPage victim) {
  auto *victim_page = victim.page;
  victim_page->WLock();
  victim_page->Unpin();
  victim_page->SetId(victim.page_id);
  victim_page->ClearIOInProgress();
  victim_page->WUnlock();
}

bool BufferPool::PageVisit(Page *page,
                           PageIdentifier id) { // TODO  might be able to use optimistic here but i
                                                // think this is just a spinlock anyway
  page->WLock();
  if (page->GetId() == id) { // PageVisit
    // page->Pin();
    page->IncUsageCount();
    page->WUnlock();
    return true;
  }
  page->WUnlock();

  return false;
}

template <typename ReadFn>
BufferPool::PinResult BufferPool::PinForBlock(PageIdentifier id, PinOptions &opts, ReadFn read_fn) {
  DB7_ASSERT(id.pid != 0, "invalid pid");
  DB7_ASSERT(id.pid != std::numeric_limits<page_id>::max(), "invalid pid");

  u32 partIdx = partitions_.GetPartitionIdx(id);

  while (true) {
    // this function pins the frame because of the instane 5 thread race :D
    frame_idx_t frame_idx =
        partitions_.GetAndApply(id, partIdx, [this](frame_idx_t f) { pages_[f].Pin(); });
    if (partitions_.IsValid(frame_idx)) {
      Page *page = &pages_[frame_idx];
      if (PageVisit(page, id)) {
        auto pinkind = page->IsIOInProgress() ? PinKind::Foreign : PinKind::Hit;
        return {page, pinkind};
      }
      page->Unpin();
    }

    VictimPage victim = GetVictim(id, opts.strategy, true);

    frame_idx_t new_frame_idx;
    if (partitions_.Put(id, victim.frame_idx, new_frame_idx, partIdx)) {
      if (opts.pkind == PageKind::Heap) {
        victim.page->SetVersions(version_table_->Get(id));
      } else {
        victim.page->SetVersions(nullptr);
      }

      if (victim.page_id != INVALID_PAGE_IDENTFIER) {
        partitions_.Delete(victim.page_id, victim.frame_idx);
      }

      // TODO at this point could be removed
      if (!read_fn(victim.page, id)) {
        UndoState(victim);
        throw MEMORY_EXCEPTION("Failure inside disk scheduler");
      }

      // caller must submit the read
      return {victim.page, PinKind::Miss};
    }

    UndoState(victim);
    continue;
  }
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
  if constexpr (DB7_BUFFER_POOL_STATS) {}
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
  u32 count = 0;
  u32 misses = 0;
  while (count < range.size) {
    PinResult res = PinForBlock(PageIdentifier(range.first.tbl_id, range.first.pid + count), opts);
    out[count] = res.page;
    count++;
    if (res.kind != PinKind::Miss)
      break; // cached or being loaded: ends the run, but it's pinned, so return it
    misses++;
  }

  if (misses > 0) {
    bool res =
        disk_mng_->Enqueue(IoTask(IoTask::READV, IoPriority::HIGH, out, misses, range.first));
    if (!res) {
      // TODO this leaks the pins
      throw MEMORY_EXCEPTION("Failed to enqueue a task to camel queue");
    }
  }
  return {count, misses > 0};
}

void BufferPool::Unpin(Page *page, bool dirty) {
  (void)dirty;
  // page->WLock();
  if (dirty) { page->SetDirty(); }
  page->Unpin();
  // page->WUnlock();
}

Page *BufferPool::Reserve(table_id tbl_id, AccessStrategy &strategy) {
  page_id pid = FreeSpaceManagerIndex::Get(tbl_id);
  // TODO just temporary guard
  DB7_ASSERT(pid < BUFFER_POOL_PAGE_NUM, "no more pages");

  auto id = PageIdentifier(tbl_id, pid);

  VictimPage victim = GetVictim(id, strategy, false);

  frame_idx_t new_frame_idx;
  if (partitions_.Put(id, victim.frame_idx, new_frame_idx)) {
    if (victim.page_id != INVALID_PAGE_IDENTFIER) {
      partitions_.Delete(victim.page_id, victim.frame_idx);
      victim.page->SetVersions(nullptr);
    }
    std::memset(victim.page->GetData(), 0, DB7_PAGE_SIZE); // TODO i dont needd this
    return victim.page;
  }

  DB7_UNREACHABLE();
}

VictimPage BufferPool::GetVictim(PageIdentifier id, AccessStrategy &strategy, bool isIO) {
  auto pages_span = GetPages();
  if (strategy.IsDefaultType()) { return replacement_.GetVictim(pages_span, id, isIO); }
  while (true) {
    auto reused = strategy.TryReuse(pages_span, id, isIO);
    if (reused.needs_flush) {
      if (!SafeFlush(reused.page)) { throw MEMORY_EXCEPTION("Disk scheduler err"); }
      continue;
    } else if (reused.page != nullptr) {
      return reused;
    }

    auto victim = replacement_.GetVictim(pages_span, id, isIO);
    DB7_ASSERT(victim.needs_flush == false, "Clock doesnt use flushing");
    strategy.Track(victim.page);
    return victim;
  }
}

bool BufferPool::SafeFlush(Page *page) {
  if (disk_mng_->Enqueue(IoTask(IoTask::WRITE, IoPriority::HIGH, page, page->GetId()))) {
    return true;
  }
  // undo path
  page->WLock();
  page->ClearWriteInProgress();
  page->SetDirty();
  page->WUnlock();
  return false;
}

} // namespace db7::storage