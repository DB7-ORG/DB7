#pragma once

#include <cassert>
#include <stdexcept>

/**
 * Disables copy constructor because of hidden copies that can occur in c++.
 * Must use default move because when u disable copy ctor compiler doesnt generate move.
 * For now not aware of better way to do this.
 */
#define DB7_DISALLOW_COPY(ClassName)                  \
    ClassName(const ClassName &) = delete;            \
    ClassName &operator=(const ClassName &) = delete; \
    ClassName(ClassName &&) = default;                \
    ClassName &operator=(ClassName &&) = default;

#define DB7_ASSERT(expr, message) assert((expr) && (message))

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x) __builtin_expect(!!(x), 1)
