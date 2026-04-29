#pragma once

#include "storage_common.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>

#define DIRTY_FLAG (1 << 0)
#define IO_IN_PROGRESS_FLAG (1 << 1)

namespace db7::storage
{
    class Page
    { // TODO padding
    private:
        PageIdentifier id_;          // 8 bytes
        byte *data_;                 // 8 bytes
        std::atomic<u32> ref_count_; // 4 bytes
        u8 flags_;                   // 1 byte
        u8 pad_[3];                  // 3 bytes padding

        alignas(CACHE_LINE_SIZE) std::shared_mutex latch_; // ~56 bytes typically
        alignas(CACHE_LINE_SIZE) std::condition_variable_any io_cv_;
        alignas(CACHE_LINE_SIZE) std::shared_mutex lock_;

    public:
        Page() : id_(0), data_(nullptr), ref_count_(0), flags_(0) {}

        /**
         * Data
         */
        byte *GetData() { return data_; }
        void SetData(byte *data) { data_ = data; }

        byte *GetOffset(u32 offset)
        {
            DB7_ASSERT(data_ != nullptr, "Page data in null");
            return data_ + offset;
        }

        template <typename T>
        void WriteOffset(u32 offset, T value)
        {
            DB7_ASSERT(data_ != nullptr, "Page data in null");
            memcpy(data_ + offset, &value, sizeof(T)); // TODO Compiler should optimize this for const fixed types, but test it.
        }

        void WriteOffset(u32 offset, byte *value, u32 size)
        {
            DB7_ASSERT(data_ != nullptr, "Page data in null");
            memcpy(data_ + offset, value, size);
        }

        /**
         * ID
         */
        PageIdentifier GetId() const { return id_; }
        void SetId(PageIdentifier id) { id_ = id; }

        /**
         * Flags
         */
        void Pin() { ref_count_.fetch_add(1); }
        void Unpin() { ref_count_.fetch_sub(1); }
        u32 PinCount() const { return ref_count_.load(); }
        bool IsPinned() const { return PinCount() > 0; }

        bool IsDirty() const { return flags_ & DIRTY_FLAG; }
        void SetDirty() { flags_ |= DIRTY_FLAG; }
        void ClearDirty() { flags_ &= ~DIRTY_FLAG; }

        bool IsIOInProgress() const { return flags_ & IO_IN_PROGRESS_FLAG; }
        void SetIOInProgress() { flags_ |= IO_IN_PROGRESS_FLAG; }
        void ClearIOInProgress() { flags_ &= ~IO_IN_PROGRESS_FLAG; }

        bool IsEvictable() const { return !IsPinned() && !IsDirty() && !IsIOInProgress(); }

        /**
         * Locks
         */
        void RLock() { latch_.lock_shared(); }
        void RUnlock() { latch_.unlock_shared(); }
        bool TryRLock() { return latch_.try_lock_shared(); }
        void WLock() { latch_.lock(); }
        void WUnlock() { latch_.unlock(); }
        bool TryWLock() { return latch_.try_lock(); }

        /**
         * Channel
         */
        void WaitIO() // TODO
        {
            if (!IsIOInProgress())
                return;

            io_cv_.wait(latch_, [&]
                        { return !IsIOInProgress(); });
        }

        void SignalIO()
        {
            ClearIOInProgress();
            io_cv_.notify_all();
        }
    };
}
