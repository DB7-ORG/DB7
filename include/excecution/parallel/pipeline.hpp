#pragma once

#include "storage/models/parallel_scan_shared.hpp"

#include <memory>

namespace db7::execution::parallel {
class Pipeline {
private:
  void *source_;
  void *code_;
  std::unique_ptr<storage::ParallelScanShared> shared;
  std::vector<Pipeline *> parents_;
  u32 id_;
  u32 ndeps_;

public:
};
} // namespace db7::execution::parallel