#pragma once

#include "storage/storage_common.hpp"

#include <algorithm>
#include <atomic>

namespace db7::storage {

struct ScanChunk {
  idx_t start = 0;
  idx_t end = 0;
};

struct ParallelScanShared {
  const idx_t ntotal;      // pages in the range, snapshotted at init
  const idx_t morsel_size; // multiple of IO_MAX_COMBINE
  const table_id tbl_id;
  alignas(64) std::atomic<idx_t> curr{1};

  ParallelScanShared(table_id t, idx_t n, idx_t m) : ntotal(n), morsel_size(m), tbl_id(t) {}

  void Stop() { curr.store(ntotal, std::memory_order_relaxed); }
  double Progress() const {
    return double(std::min(curr.load(std::memory_order_relaxed), ntotal)) / ntotal;
  }

  bool Next(ScanChunk &m) {
    idx_t start = curr.fetch_add(morsel_size, std::memory_order_relaxed);
    if (start >= ntotal) return false;
    m = {start, std::min(start + morsel_size, ntotal)};
    return true;
  }
};
} // namespace db7::storage