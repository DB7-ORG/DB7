#pragma once

#include "storage/buffer_pool/iostream.hpp"
#include "storage/models/parallel_scan_shared.hpp"

namespace db7::storage {

struct ScanCursor {
  ParallelScanShared &shared;
  ScanChunk cursor;

  explicit ScanCursor(ParallelScanShared &sh) : shared(sh) {}

  page_id operator()() { // what the stream calls as next_fn_()
    if (cursor.start == cursor.end && !shared.Next(cursor)) return INVALID_BLOCK;
    return page_id(cursor.start++);
  }
};

class ScanLocal {
private:
  AccessStrategy strategy_;
  ReadIoStream<ScanCursor> stream_;

public:
  ScanLocal(BufferPool *pool, ParallelScanShared &shared, u32 max_distance)
      : strategy_(BULK_READ),
        stream_(pool, strategy_, shared.tbl_id, ScanCursor(shared), max_distance) {}

  ScanLocal(const ScanLocal &) = delete;
  ScanLocal &operator=(const ScanLocal &) = delete;

  ReadIoStream<ScanCursor> &Stream() { return stream_; }
};
} // namespace db7::storage