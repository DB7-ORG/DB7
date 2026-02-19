#pragma once

#include "common.hpp"

template <typename ValueType>
inline u32 CountBitsUsed(ValueType value)
{
    return value == 0 ? 1 : 32 - __builtin_clz(value);
}