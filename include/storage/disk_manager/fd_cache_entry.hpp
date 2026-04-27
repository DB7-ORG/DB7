#pragma once

#include "storage/storage_common.hpp"

namespace db7::storage
{
    struct FdCacheEntry
    {
        table_id tbl_id;
        int fd;          /* OS file descriptor, -1 = unused slot    */
        u64 last_access; /* monotonic counter for LRU eviction      */

        FdCacheEntry *lru_prev;
        FdCacheEntry *lru_next;
    };
}