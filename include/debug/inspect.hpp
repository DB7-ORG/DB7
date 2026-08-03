#pragma once

#ifdef DB7_DEBUG_FLAG

#include "common.hpp"
#include "access/index/layouts/varlen/varlen_layout_models.hpp"
#include "storage/storage_common.hpp"

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
        char prefix_key[256];
        u16 prefix_len;
    };

    struct ColumnVal
    {
        char name[64];
        char val[1024];
    };

    struct HeapDump
    {
        u32 row_count;
        u32 column_count;
        bool deleted[1024];          // per row
        ColumnVal columns[1024][64]; // [row][col]
    };

    enum LayoutType
    {
        Index,
        Table,
        Unknown
    };

    extern "C" HeapDump *InspectHeapLayout(byte *data, table_id tbl_id);

    extern "C" PageDump *InspectVarlenLayout(byte *data);

    extern "C" LayoutType GetLayoutType(table_id tbl_id);
}

#endif