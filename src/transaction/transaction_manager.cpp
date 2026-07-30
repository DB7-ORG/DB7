#include "transaction/transaction_manager.hpp"

#include <algorithm>
#include <vector>

namespace db7::transaction
{
    TransactionContext *TransactionManager::BeginTransaction()
    {
        timestamp_t start_time = timestamp_manager_->BeginTransaction();
        TransactionContext *result = new TransactionContext(start_time, start_time + INT64_MIN, buffer_pool_, version_manager_, mem_pool_);
        commit_latch_.Try();
        return result;
    }

    timestamp_t TransactionManager::Commit(TransactionContext *txn)
    {
        DB7_ASSERT(!txn->GetState(), "Txn shouldnt be aborted");
        // TODO if its not read-only txn
        shared::AdaptiveSpinLock::SpinGuard guard(commit_latch_);
        timestamp_t commit_time = timestamp_manager_->CheckOutTimestamp();
        txn->RestampVersions(commit_time);
        return commit_time;
    }
}