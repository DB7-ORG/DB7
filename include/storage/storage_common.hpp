#pragma once

#include "common.hpp"

#define PAGE_SIZE (1 << 18)
#define BUFFER_POOL_PAGE_NUM 1000
#define BUFFER_POOL_PARTITION_NUM 128
#define IOURING_QUEUE_SIZE 512

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

        explicit PageIdentifier(table_id tbl_id, page_id pid)
            : tbl_id(tbl_id), pid(pid) {}

        explicit PageIdentifier(u64 packed)
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