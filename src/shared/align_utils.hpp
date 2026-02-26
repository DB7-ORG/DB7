#pragma once

#include "common.hpp"
#include <stddef.h>
#include <limits>

inline u32 RoundUp(u32 val, u32 alignment)
{
    return (val + alignment - 1) / alignment;
}
