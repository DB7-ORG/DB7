#pragma once

#include "common.hpp"

namespace db7::storage
{
    struct VarlenEntry
    {
        byte *content;
        u32 size;
        u32 prefix;
    };
    static_assert(sizeof(VarlenEntry) == 16);
}