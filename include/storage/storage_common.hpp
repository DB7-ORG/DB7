#pragma once

#include "common.hpp"

#define DB7_PAGE_SIZE (1 << 13)
#define DB7_MAX_ROW_SIZE DB7_PAGE_SIZE / 10
#define BUFFER_POOL_PAGE_NUM 80000
#define BUFFER_POOL_PARTITION_NUM 128
#define IOURING_QUEUE_SIZE 512

using page_id = uint32_t;
using table_id = uint32_t;
using store_column_id = uint32_t;

namespace db7::storage
{
    /**
     * If u modify this make sure to change hashing logic
     */
    struct PACKED PageIdentifier
    {
        union
        {
            struct
            {
                table_id tbl_id;
                page_id pid;
            };
            u64 packed;
        };

        PageIdentifier() {}

        PageIdentifier(table_id tbl_id, page_id pid)
            : tbl_id(tbl_id), pid(pid) {}

        PageIdentifier(u64 packed)
            : packed(packed) {}

        bool operator==(const PageIdentifier &other) const
        {
            return packed == other.packed;
        }
    };
}

namespace std
{
    template <>
    struct hash<db7::storage::PageIdentifier>
    {
        size_t operator()(const db7::storage::PageIdentifier &p) const
        {
            return hash<uint64_t>{}(p.packed);
        }
    };
}