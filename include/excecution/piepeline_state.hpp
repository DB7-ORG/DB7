#pragma once

#include "storage/models/parallel_scan_shared.hpp"

namespace db7::execution {
struct PipelineContext {
  storage::ParallelScanShared cursor;
  void *source;
  void *code;
  u32 pipeline_id;
};
} // namespace db7::execution