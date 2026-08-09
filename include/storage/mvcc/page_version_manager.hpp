#pragma once

#include "shared/locks/adaptive_version_lock.hpp"
#include "storage/storage_common.hpp"
#include "storage/mvcc/version_ptr.hpp"

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

            auto it = table_.find(new_page_id);
            if (it != table_.end())
            {
                return it->second;
            }

            return nullptr;
        }

        VersionPtr *InitializeVersions(PageIdentifier id, u32 count)
        {
            shared::AdaptiveVersionLock::WriteGuard guard(lock_);
            auto *result = new storage::VersionPtr[count]();
            table_[id] = result;
            return result;
        }

        u64 ValidateVersion()
        { // TODO fix index
            return 1;
            // return transaction::TransactionUtil::HasConflict(version_ptr->GetTimestamp(), txn->FinishTime(), txn->StartTime());
        }
    };
}