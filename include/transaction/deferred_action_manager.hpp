#pragma once

#include "transaction/timestamp_manager.hpp"

namespace db7::transaction {
class DeferredActionManager {
private:
  TimestampManager *timestamp_manager_;

public:
  DeferredActionManager(TimestampManager *timestamp_manager)
      : timestamp_manager_(timestamp_manager) {}
};
} // namespace db7::transaction