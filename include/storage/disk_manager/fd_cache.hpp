#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"

#include <shared_mutex>
#include <mutex>

#define TABLE_FILE_HEADER_SIZE 16

#define MAX_OPEN_FILES 128u
using file_t = int;

namespace db7::storage
{

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