#pragma once

#include "common.hpp"

namespace db7::transaction
{
    using timestamp_t = u64;

    enum DurabilityPolicy
    {
        DISABLED = 0,
        SYNC
    };
}