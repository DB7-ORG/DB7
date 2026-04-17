#pragma once

namespace db7::shared
{

    template <typename T>
    constexpr T AlignUp(T value, T alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }
}