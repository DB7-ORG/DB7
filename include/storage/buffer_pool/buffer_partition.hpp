#pragma once

#include "storage/storage_common.hpp"

#include <unordered_map>
#include <shared_mutex>
#include <mutex>

namespace db7::storage
{
    class alignas(64) BufferPartition
    {
    private:
        std::unordered_map<page_id, u32> hmap;
        std::shared_mutex lock;

    public:
        u32 Get(page_id page_id)
        {
            std::shared_lock guard(lock);
            auto it = hmap.find(page_id);
            if (it != hmap.end())
            {
                return it->second;
            }
            return UINT32_MAX;
        };

        bool Put(page_id page_id, u32 frame_idx, u32 &new_frame_idx) // TODO return idx of inserted frame
        {
            std::unique_lock guard(lock);
            auto [it, inserted] = hmap.emplace(page_id, frame_idx);
            new_frame_idx = it->second;
            return inserted;
        }

        void Delete(page_id evict_page_id, u32 old_frame_idx)
        {
            std::unique_lock guard(lock);
            auto it = hmap.find(evict_page_id);
            if (it != hmap.end() && it->second == old_frame_idx)
            {
                hmap.erase(it);
            }
        }
    };
}