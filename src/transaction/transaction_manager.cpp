#include "transaction/transaction_manager.hpp"

#include <algorithm>
#include <vector>

namespace db7::transaction
{
    TransactionContext *TransactionManager::BeginTransaction()
    {
        timestamp_t start_time = timestamp_manager_->BeginTransaction();
        TransactionContext *result = new TransactionContext(start_time, start_time + INT64_MIN, buffer_pool_, version_manager_, mem_pool_);
        // lock for commit txns
        return result;
    }

    timestamp_t TransactionManager::Commit(TransactionContext *txn)
    {
        DB7_ASSERT(!txn->GetState(), "Txn should be aborted");
        timestamp_manager_->Commit();
        return 0;
    }
}