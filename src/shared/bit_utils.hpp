#pragma once

#include "common.hpp"

template <typename ValueType>
inline u32 CountBitsUsed(ValueType value)
{
    static_assert(std::is_unsigned_v<ValueType>);
    return value == 0 ? 1 : sizeof(ValueType) * CHAR_BIT - std::countl_zero(value);
}