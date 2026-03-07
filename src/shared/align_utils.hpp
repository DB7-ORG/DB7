#pragma once

#include "common.hpp"
#include <stddef.h>
#include <limits>

inline u32 RoundUp(u32 val, u32 alignment)
{
    return ((val + alignment - 1) / alignment) * alignment;
}

inline bool IsPow2(u32 val)
{
    return val > 0 && (val & (val - 1)) == 0;
}

template <typename T>
inline T *AlignUp(T *ptr)
{
    auto alignment = alignof(T);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t new_addr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<T *>(new_addr);
}