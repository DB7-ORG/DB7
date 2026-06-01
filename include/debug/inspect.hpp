#pragma once

#ifdef DEBUG

#include "common.hpp"
#include "access/index/varlen_layout/btree_varlen_models.hpp"

namespace db7::debug
{

    struct SlotDump
    {
        u32 slot;
        u32 offset;
        u16 len;
        u64 result_val;
        char key[256];
    };

    struct PageDump
    {
        page_id pid;
        u64 rlink;
        u64 llink;
        u32 count;
        u8 level;
        u64 max_val;
        u32 heap_size;
        char max_val_key[256];
        SlotDump slots[1024];
    };

    extern "C" PageDump *InspectVarlenLayout(byte *data);
}

#endif