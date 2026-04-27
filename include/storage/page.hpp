#pragma once

#include "storage_common.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>
#include <shared_mutex>
#include <atomic>

namespace db7::storage
{
    struct Page
    {
        page_id pid;
        std::atomic<u32> ref_count;
        std::shared_mutex latch; // header lock
        // padding
        u64 none;
        std::shared_mutex lock;
        byte *data;

        byte *GetOffset(u32 offset)
        {
            DB7_ASSERT(data != nullptr, "Page data in null");
            return data + offset;
        }

        template <typename T>
        void WriteOffset(u32 offset, T value)
        {
            DB7_ASSERT(data != nullptr, "Page data in null");
            memcpy(data + offset, &value, sizeof(T)); // TODO Compiler should optimize this for const fixed types, but test it.
        }

        void WriteOffset(u32 offset, byte *value, u32 size)
        {
            DB7_ASSERT(data != nullptr, "Page data in null");
            memcpy(data + offset, value, size);
        }

        /**
         * Lock utils so i can change the lock type later
         */
        void RLock()
        {
            latch.lock_shared();
        }

        void RUnlock()
        {
            latch.unlock_shared();
        }

        bool TryRLock()
        {
            return latch.try_lock_shared();
        }

        void WLock()
        {
            latch.lock();
        }

        void WUnlock()
        {
            latch.unlock();
        }

        bool TryWLock()
        {
            return latch.try_lock();
        }
    };
}