#pragma once

#include "storage/storage_common.hpp"
#include "third_party/unordered_dense.h"

#include <shared_mutex>
#include <mutex>

namespace db7::storage
{
    class BufferPartitions
    {
    private:
        struct alignas(64) AlignedLock
        {
            std::shared_mutex lock;
        };
        ankerl::unordered_dense::map<PageIdentifier, u32> partition_maps_[BUFFER_POOL_PARTITION_NUM];
        AlignedLock partition_locks_[BUFFER_POOL_PARTITION_NUM];

    public:
        void Reserve(size_t n, u32 partIdx)
        {
            std::shared_lock guard(partition_locks_[partIdx].lock);
            partition_maps_[partIdx].reserve(n);
        }

        u32 Get(PageIdentifier id, u32 partIdx)
        {
            auto &partition = partition_maps_[partIdx];
            std::shared_lock guard(partition_locks_[partIdx].lock);
            auto it = partition.find(id);
            if (it != partition.end())
            {
                return it->second;
            }
            return UINT32_MAX;
        };

        bool Put(PageIdentifier id, u32 frame_idx, u32 &new_frame_idx, u32 partIdx)
        {
            auto &partition = partition_maps_[partIdx];
            std::unique_lock guard(partition_locks_[partIdx].lock);
            auto [it, inserted] = partition.emplace(id, frame_idx);
            new_frame_idx = it->second;
            return inserted;
        }

        void Delete(PageIdentifier evict_page_id, u32 old_frame_idx, u32 partIdx)
        {
            auto &partition = partition_maps_[partIdx];
            std::unique_lock guard(partition_locks_[partIdx].lock);
            auto it = partition.find(evict_page_id);
            if (it != partition.end() && it->second == old_frame_idx)
            {
                partition.erase(it);
            }
        }
    };
}