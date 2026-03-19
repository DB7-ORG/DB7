#pragma once

#include "common.hpp"
#include <memory>

template <typename T>
struct FixedDeque
{
    std::unique_ptr<T[]> applied;
    T *rawPtr;
    u32 first;
    u32 last;

    FixedDeque(u32 size)
    {
        applied = std::make_unique<T[]>(size);
        rawPtr = applied.get();
        first = 0;
        last = 0;
    }

    T *GetPtrRaw()
    {
        return rawPtr;
    }

    void Push(T alg)
    {
        applied[last++] = alg;
    }

    T Pop()
    {
        return applied[--last];
    }

    T PopFirst()
    {
        return applied[first++];
    }

    u32 Size()
    {
        return last - first;
    }
};