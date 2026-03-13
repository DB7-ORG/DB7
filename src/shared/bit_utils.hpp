#pragma once

#include "common.hpp"

template <typename ValueType>
inline u32 CountBitsUsed(ValueType value)
{
    static_assert(std::is_unsigned_v<ValueType>);
    return value == 0 ? 1 : sizeof(ValueType) * CHAR_BIT - std::countl_zero(value);
}

template <typename ValueType>
inline u32 CountLeadingZeros(ValueType value)
{
    static_assert(std::is_unsigned_v<ValueType>);
    return std::countl_zero(value);
}

template <typename T>
u64 ToU64Bits(T val)
{
    u64 result = 0;
    memcpy(&result, &val, sizeof(T));
    return result;
}