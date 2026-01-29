#ifndef ALIGN_UTIL_H
#define ALIGN_UTIL_H

#include <stddef.h>

template <typename T>
constexpr T align_up_pow2(T x, size_t align)
{
    return (x + align - 1) & ~(align - 1);
}

#endif