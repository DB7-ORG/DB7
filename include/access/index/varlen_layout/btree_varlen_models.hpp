#pragma once

#include "common.hpp"

namespace db7::access
{
    struct Key
    {
        u16 len;
        byte *data;
    };

    struct VarlenHeader
    {
        u32 heap_size; // taken heap space
        // u32 total_taken; // heap_size + taken slot size + headers //TODO optimization for Free space calc
    };

    struct Slot
    {
        u32 offset;
    };

    template <typename R>
    struct SlotValHeader
    {
        R result;
        u16 len;
    };

    template <typename R>
    struct SlotVal
    {
        SlotValHeader<R> hdr;
        byte *data;
    };
};