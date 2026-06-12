#pragma once

#include "storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/locks/adaptive_version_lock.hpp"
#include "storage/mvcc/mapping_table_manager.hpp"

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
        std::atomic<PageIdentifier> id_;     // 8 bytes
        std::atomic<VersionPtr *> versions_; // 8 bytes
        byte *data_;                         // 8 bytes
        std::atomic<u32> ref_count_;         // 4 bytes
        std::atomic<u8> flags_;              // 1 byte
        u8 pad_[3];                          // 3 bytes padding

        alignas(CACHE_LINE_SIZE) std::mutex latch_; // ~56 bytes typically
        std::condition_variable_any io_cv_;
        alignas(CACHE_LINE_SIZE) shared::AdaptiveVersionLock lock_;

    public:
        Page() : id_(PageIdentifier{0}), data_(nullptr), ref_count_(0), flags_(0) {}

        VersionPtr *GetVersions() { return versions_.load(); }
        void SetVersions(VersionPtr *ptr) { versions_.store(ptr); }
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

        void WriteOffset(u32 offset, const byte *value, u32 size)
        {
            DB7_ASSERT(data_ != nullptr, "Page data in null");
            memcpy(data_ + offset, value, size);
        }

        /**
         * ID
         */
        PageIdentifier GetId() const { return id_; }
        page_id GetPageId() const { return id_.load().pid; }
        void SetId(PageIdentifier id) { id_ = id; }

        /**
         * Flags
         */
        void Pin() { ref_count_++; }
        void Unpin() { ref_count_--; }
        u32 PinCount() const { return ref_count_; }
        bool IsPinned() const { return PinCount() > 0; }

        bool IsIOInProgress() const { return flags_ & IO_IN_PROGRESS_FLAG; }
        // void SetIOInProgress() { flags_ |= IO_IN_PROGRESS_FLAG; }
        // void ClearIOInProgress() { flags_ &= ~IO_IN_PROGRESS_FLAG; }
        void SetIOInProgress() { flags_.fetch_or(IO_IN_PROGRESS_FLAG); }
        void ClearIOInProgress() { flags_.fetch_and(~IO_IN_PROGRESS_FLAG); }

        bool IsDirty() const { return flags_ & DIRTY_FLAG; }
        // void SetDirty() { flags_ |= DIRTY_FLAG; }
        // void ClearDirty() { flags_ &= ~DIRTY_FLAG; }
        void SetDirty() { flags_.fetch_or(DIRTY_FLAG); }
        void ClearDirty() { flags_.fetch_and(~DIRTY_FLAG); }

        bool IsEvictable() const { return !IsPinned() && !IsDirty() && !IsIOInProgress(); }

        /**
         * Locks
         */
        // void RLock() { latch_.lock_shared(); }
        // void RUnlock() { latch_.unlock_shared(); }
        // bool TryRLock() { return latch_.try_lock_shared(); }
        void WLock() { latch_.lock(); }
        void WUnlock() { latch_.unlock(); }
        bool TryWLock() { return latch_.try_lock(); }

        void RDataLock() { lock_.ReadLock(); }
        void RDataUnlock() { lock_.ReadUnlock(); }
        void WDataLock() { lock_.WriteLock(); }
        void WDataUnlock() { lock_.WriteUnlock(); }
        bool ReadVersion(u64 &version) { return lock_.ReadOptimistic(version); }
        bool ValidateVersion(u64 version) { return lock_.Validate(version); }

        /**
         * Channel
         */
        void WaitIO()
        {
            if (!IsIOInProgress())
                return;

            std::unique_lock lk(latch_);
            io_cv_.wait(lk, [&]
                        { return !IsIOInProgress(); });
        }

        void SignalIO()
        {
            {
                std::lock_guard lk(latch_);
                DB7_ASSERT(IsIOInProgress(), "Io in progress not set");
                DB7_ASSERT(IsPinned(), "Pin not set");
                // DB7_ASSERT(GetId().pid == *(u64 *)GetData(), "Invalid page");
                ClearIOInProgress();
            }
            io_cv_.notify_all();
        }
    };
}
