#include "transaction/transaction_manager.hpp"

#include <algorithm>
#include <vector>

namespace db7::transaction
{
    TransactionContext *TransactionManager::BeginTransaction()
    {
        timestamp_t start_time = timestamp_manager_->BeginTransaction();
        TransactionContext *result = new TransactionContext(start_time, start_time + INT64_MIN, buffer_pool_);
        return result;
    }
}