#pragma once

#include "common.hpp"

namespace db7::storage
{
    using page_id = u32;
    using table_id = u32;
    constexpr u32 HEADER_SIZE = 64;
    constexpr u32 PAGE_SIZE = 1 << 20;
    constexpr u32 PAYLOAD_SIZE = PAGE_SIZE - HEADER_SIZE;
    constexpr u32 BUFFER_POOL_PAGE_NUM = 2 * 1024;
    constexpr u32 BUFFER_POOL_PARTITION_NUM = 2;

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