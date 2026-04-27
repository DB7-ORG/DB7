#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"

#include <shared_mutex>
#include <mutex>

#define TABLE_FILE_HEADER_SIZE 16
#define MIN_HEADER_PAGE_SIZE 4096

#define MAX_OPEN_FILES 128u
using file_t = int;

namespace db7::storage
{
    // struct PACKED TableFileHeader
    // {
    //     u32 magic;
    //     table_id tbl_id;
    //     u32 page_count;     /* includes the header page itself */
    //     u32 free_page_head; /* page_id of first free page, 0 = none */
    //     char reserved[MIN_HEADER_PAGE_SIZE - TABLE_FILE_HEADER_SIZE];
    // };

    struct FdCacheEntry
    {
        file_t fd;
        u32 page_count;

        FdCacheEntry() : fd(-1), page_count(0) {}

        FdCacheEntry(file_t fd, u32 page_count) : fd(fd), page_count(page_count) {}
    };

    class FdCache
    {
    private:
        FdCacheEntry cache_[MAX_OPEN_FILES];
        std::shared_mutex latch;

    public:
        FdCacheEntry Get(u32 idx)
        {
            DB7_ASSERT(idx < MAX_OPEN_FILES, "Out of range idx");
            std::shared_lock guard(latch);
            return cache_[idx];
        }

        void Set(u32 idx, FdCacheEntry entry)
        {
            DB7_ASSERT(idx < MAX_OPEN_FILES, "Out of range idx");
            std::unique_lock guard(latch);
            cache_[idx] = entry;
        }

        FdCacheEntry Invalidate(u32 idx)
        {
            DB7_ASSERT(idx < MAX_OPEN_FILES, "Out of range idx");
            std::unique_lock guard(latch);
            auto entry = cache_[idx];
            cache_[idx].fd = -1;
            cache_[idx].page_count = 0;
            return entry;
        }
    };
}