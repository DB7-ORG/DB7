#pragma once

#include "common.hpp"
#include "shared/data_structures/ring_buffer.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/page.hpp"

namespace db7::storage {
inline static constexpr u32 INVALID_BLOCK = ~0u;

enum StreamType { SCAN, SCAN_UNORDERED };

class IoStream {
private:
  StreamType type_;
  table_id table_id_;

public:
  IoStream(StreamType type, table_id table_id) : type_(type), table_id_(table_id) {}
  StreamType GetType() const { return type_; }
  table_id GetTableId() const { return table_id_; }

  virtual ~IoStream() = default;
  virtual Page *Next() = 0;
};

/**
 * Ordered read stream over the pages of one table.
 * @param NextFn returns the next block number to read, or INVALID_BLOCK when there are no more.
 * It is never called again after returning INVALID_BLOCK.
 * Not thread safe. Not copyable or movable: it owns pins.
 */
template <typename NextFn>
class ReadIoStream final : public IoStream {
  static_assert(IO_MAX_COMBINE <= 255, "batch_size_ is stored as u8");

private:
  shared::Ringbuffer<Page *> ring_;
  BufferPool *pool_;
  AccessStrategy strategy_;
  [[no_unique_address]] NextFn next_fn_; // takes no space when the callback captures nothing

  u32 distance_ = 1;      // current look-ahead (Postgres: readahead_distance)
  u32 max_distance_;      // maximum distance u can read ahead
  u32 decay_holdoff_ = 0; // don't shrink right after real I/O or a wait
  u32 buffered_;          // next block from the callback that is NOT in the pending run yet
  u32 pending_start_ = 0; // contiguous run taken from the callback but not pinned yet
  u32 pending_len_ = 0;   // (Postgres: pending_read_blocknum / pending_read_nblocks)

  u8 batch_size_;
  bool done_ = false;

  // The I/O layer must copy these pointers; this array is overwritten on the next PinRange.
  Page *pages_[IO_MAX_COMBINE];

  // Grow the pending run with contiguous blocks, up to batch_size_.
  void FillPendingRun() {
    while (pending_len_ < batch_size_ && buffered_ != INVALID_BLOCK) {
      if (pending_len_ == 0) {
        pending_start_ = buffered_;
      } else if (buffered_ != pending_start_ + pending_len_) {
        break; // not contiguous: buffered_ will start the next run
      }
      pending_len_++;
      buffered_ = next_fn_();
    }
  }

  void LookAhead() {
    while (!done_ && ring_.Size() < distance_) {
      FillPendingRun();
      if (pending_len_ == 0) {
        done_ = true;
        return;
      }

      DB7_ASSERT(pending_len_ <= ring_.MaxSize() - ring_.Size(), "Ring has no room for the run");

      auto r = pool_->PinRange(GetTableId(), pending_start_, pending_len_, pages_, true);
      DB7_ASSERT(r.count > 0 && r.count <= pending_len_, "PinRange must pin at least one page");

      if (r.io_started) {
        decay_holdoff_ = max_distance_; // miss: real I/O in this region, don't shrink
      } else if (decay_holdoff_ > 0) {  //
        decay_holdoff_--;               // hit: decay counter
      } else if (distance_ > 1) {       //
        distance_--;                    // hit: look-ahead isn't buying anything
      }

      ring_.Append(pages_, r.count);

      // whatever PinRange didn't take stays pending and is topped up next iteration
      pending_start_ += r.count;
      pending_len_ -= r.count;
    }
  }

public:
  ReadIoStream(BufferPool *pool, AccessStrategy strategy, table_id tbl, NextFn next_fn,
               u32 max_distance, u32 batch_size = 8)
      : IoStream(StreamType::SCAN, tbl), ring_(max_distance + batch_size), pool_(pool),
        strategy_(strategy), next_fn_(std::move(next_fn)), max_distance_(max_distance),
        batch_size_(static_cast<u8>(batch_size)) {
    DB7_ASSERT(max_distance >= 1, "max_distance must be at least 1");
    DB7_ASSERT(batch_size > 0 && batch_size <= IO_MAX_COMBINE, "Invalid batch size");
    buffered_ = next_fn_(); // an empty stream is detected by the first LookAhead
  }

  ReadIoStream(const ReadIoStream &) = delete;
  ReadIoStream &operator=(const ReadIoStream &) = delete;

  ~ReadIoStream() override { // scan ended early (LIMIT, error)
    for (auto r = ring_.Read(); r.success; r = ring_.Read()) pool_->Unpin(r.value);
  }

  // Returns a pinned page; the caller must Unpin it. Returns nullptr at the end.
  Page *Next() override {
    if (ring_.Size() == 0) {
      LookAhead();
      if (ring_.Size() == 0) return nullptr;
    }

    Page *page = ring_.ReadUnsafe();

    LookAhead(); // refill the slot we just freed before possibly blocking

    if (page->IsIOInProgress()) { // look-ahead didn't hide the latency: look further ahead
      distance_ = std::min(distance_ * 2, max_distance_);
      decay_holdoff_ = max_distance_;
      page->WaitIO();
    }

    return page;
  }
};

// /* Not thread safe */
// class ReadUnorderedIoStream : public IoStream {
// private:
//   u32 nblocks_;           // pages in the table
//   u32 next_block_ = 0;    // next block to pin (Postgres: rs_prefetch_block)
//   u32 distance_ = 1;      // current look-ahead (Postgres: readahead_distance)
//   u32 decay_holdoff_ = 0; // don't shrink right after we had to wait
//   u32 count_ = 0;
//   shared::Ringbuffer<Page *> wait_ring_;
//   shared::Ringbuffer<Page *> ready_ring_;
//   BufferPool *pool_;

//   void LookAhead() {
//     while (count_ < distance_ && next_block_ < nblocks_) {
//       auto *page = pool_->Pin({GetTableId(), next_block_++});
//       count_++;
//       if (!page->IsIOInProgress()) {
//         if (decay_holdoff_ > 0)
//           decay_holdoff_--;
//         else if (distance_ > 1)
//           distance_--; // cached: look-ahead isn't buying anything

//         ready_ring_.Append(page);
//       } else {
//         wait_ring_.Append(page);
//       }
//     }
//   }

//   u32 Count() const { return ready_ring_.Size() + wait_ring_.Size(); }

// public:
//   ReadUnorderedIoStream(BufferPool *pool, table_id tbl, u32 nblocks, u32 max_distance)
//       : IoStream(StreamType::SCAN_UNORDERED, tbl), nblocks_(nblocks), wait_ring_(32),
//         ready_ring_(32), pool_(pool) {}

//   ~ReadUnorderedIoStream() override { // scan ended early (LIMIT, error)
//     for (auto r = wait_ring_.Read(); r.success; r = wait_ring_.Read()) pool_->Unpin(r.value);
//     for (auto r = ready_ring_.Read(); r.success; r = ready_ring_.Read()) pool_->Unpin(r.value);
//   }

//   // Returns a pinned page; the caller must Unpin it. Returns nullptr at the end.
//   Page *Next() override {
//     if (count_ == 0) {
//       LookAhead();
//       if (count_ == 0) return nullptr;
//     }

//     auto result = ready_ring_.Read();
//     count_--;
//     if (result.success) {
//       Page *page = result.value;
//       LookAhead();
//       return page;
//     }

//     Page *page = wait_ring_.ReadUnsafe();
//     if (page->IsIOInProgress()) {
//       distance_ = std::min(distance_ * 2, wait_ring_.MaxSize());
//       decay_holdoff_ = wait_ring_.MaxSize();
//       page->WaitIO();
//     }

//     LookAhead(); // refill the slot we just freed
//     return page;
//   }
// };
} // namespace db7::storage