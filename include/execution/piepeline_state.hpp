#pragma once

#include "execution/sources/table_source.hpp"

namespace db7::execution {
struct PipelineContext {
  BaseSource *source;
  void *code;
  u32 pipeline_id;
};
} // namespace db7::execution