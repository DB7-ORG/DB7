#pragma once

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
}