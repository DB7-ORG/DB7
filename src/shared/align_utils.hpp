#pragma once

#include "common.hpp"
#include <stddef.h>
#include <limits>

template <typename T>
constexpr T RoundUp(T val, T alignment)
{
    return (val + alignment - 1) / alignment;
}
