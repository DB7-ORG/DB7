#pragma once

#include "common.hpp"

#define HEADER_SIZE 64
#define PAGE_SIZE (1 << 20)
#define PAYLOAD_SIZE (PAGE_SIZE - HEADER_SIZE)
#define BUFFER_POOL_PAGE_NUM (25)
#define BUFFER_POOL_PARTITION_NUM 2

namespace db7::storage
{
    using page_id = u32;
    using table_id = u32;

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