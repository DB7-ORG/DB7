#pragma once

#include "common.hpp"
namespace db7 {
using timestamp_t = u64;
namespace transaction {

enum DurabilityPolicy { DISABLED = 0, SYNC };
} // namespace transaction
} // namespace db7