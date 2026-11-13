#pragma once

#include "storage/models/scan_local.hpp"
namespace db7::execution {

struct SourceLocalState {
  virtual ~SourceLocalState() = default;
};

struct TableLocalState final : SourceLocalState {
  std::unique_ptr<storage::ScanLocal> scan;
};

} // namespace db7::execution