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
        std::atomic<u8> flags_;      // 1 byte
        u8 pad_[3];                  // 3 bytes padding

        alignas(CACHE_LINE_SIZE) std::shared_mutex latch_; // ~56 bytes typically
        alignas(CACHE_LINE_SIZE) std::mutex io_mtx_;
        alignas(CACHE_LINE_SIZE) std::condition_variable io_cv_;
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

        bool IsIOInProgress() const { return flags_.load(std::memory_order_acquire) & IO_IN_PROGRESS_FLAG; }
        void SetIOInProgress() { flags_.fetch_or(IO_IN_PROGRESS_FLAG, std::memory_order_release); }
        void ClearIOInProgress() { flags_.fetch_and(~IO_IN_PROGRESS_FLAG, std::memory_order_release); }

        bool IsDirty() const { return flags_.load(std::memory_order_acquire) & DIRTY_FLAG; }
        void SetDirty() { flags_.fetch_or(DIRTY_FLAG, std::memory_order_release); }
        void ClearDirty() { flags_.fetch_and(~DIRTY_FLAG, std::memory_order_release); }

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
        void WaitIO()
        {
            std::unique_lock lk(io_mtx_);
            io_cv_.wait(lk, [&]
                        { return !IsIOInProgress(); });
        }

        void SignalIO()
        {
            {
                std::lock_guard lk(io_mtx_);
                ClearIOInProgress();
            }
            io_cv_.notify_all();
        }
    };
}
