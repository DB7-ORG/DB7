#pragma once

#include "shared/macro_helper.hpp"

#include <memory>
#include <cstring>

namespace db7::shared
{

    template <typename T>
    constexpr T AlignUp(T value, T alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    constexpr byte *AlignUp(byte *value, u32 alignment)
    {
        auto addr = reinterpret_cast<uintptr_t>(value);
        addr = (addr + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
        return reinterpret_cast<byte *>(addr);
    }

    struct AlignedDeleter
    {
        void operator()(void *ptr) const noexcept { std::free(ptr); }
    };

    using AlignedPtr = std::unique_ptr<byte[], AlignedDeleter>;

    inline AlignedPtr AllocAligned(u32 size, u32 alignment)
    {
        u32 alloc_size = AlignUp(size, alignment);
        void *ptr = std::aligned_alloc(alignment, alloc_size);
        DB7_ASSERT(ptr != nullptr, "aligned_alloc failed");
        std::memset(ptr, 0, alloc_size);
        return AlignedPtr(static_cast<byte *>(ptr));
    }
}