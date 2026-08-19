#pragma once

#include "shared/locks/adaptive_version_lock.hpp"
#include "storage/storage_common.hpp"
#include "storage/mvcc/version_ptr.hpp"
#include "shared/models/tuple_id.hpp"

#include <unordered_map>
#include <atomic>

namespace db7::storage
{
    class PageVersionManager
    {
    private:
        shared::AdaptiveVersionLock lock_; // TODO should be cas hash map

        // Tracks page -> version vector
        std::unordered_map<PageIdentifier, VersionPtr *> table_;

        VersionPtr *GetUnsafe(PageIdentifier new_page_id)
        {
            auto it = table_.find(new_page_id);
            if (it != table_.end())
            {
                return it->second;
            }

            return nullptr;
        }

    public:
        PageVersionManager() = default;

        ~PageVersionManager()
        {
            for (auto &[id, versions] : table_)
                delete[] versions;
        }

        VersionPtr *Get(PageIdentifier new_page_id)
        {
            shared::AdaptiveVersionLock::WriteGuard guard(lock_);

            return GetUnsafe(new_page_id);
        }

        VersionPtr *GetCreateVersions(PageIdentifier id, u32 count)
        {
            shared::AdaptiveVersionLock::WriteGuard guard(lock_);

            auto *result = GetUnsafe(id);

            result = (result == nullptr) ? new storage::VersionPtr[count]() : result;

            table_[id] = result;

            return result;
        }

        storage::UndoRecord *GetDelta(TupleId tid, table_id tbl_id)
        {
            shared::AdaptiveVersionLock::WriteGuard guard(lock_);
            auto *versions = GetUnsafe({tbl_id, tid.GetPageId()});
            if (!versions)
                return nullptr;
            auto *undo = versions[tid.GetIndex()].Get();
            return undo;
        }
    };
}