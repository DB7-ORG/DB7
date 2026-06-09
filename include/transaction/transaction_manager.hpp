#pragma once

#include "transaction/timestamp_manager.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "transaction/transaction_context.hpp"

namespace db7::transaction
{
    class TransactionManager
    {
    private:
        TimestampManager *timestamp_manager_;
        storage::BufferPool *buffer_pool_;

    public:
        TransactionManager(
            TimestampManager *timestamp_manager,
            storage::BufferPool *buffer_pool)
            : timestamp_manager_(timestamp_manager), buffer_pool_(buffer_pool) {}

        /**
         * Begins a transaction.
         * @return transaction context for the newly begun transaction
         */
        TransactionContext *BeginTransaction();
    };
}