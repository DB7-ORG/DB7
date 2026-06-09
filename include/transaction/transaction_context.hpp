#pragma once

#include "transaction/transaction_common.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"

namespace db7::transaction
{

    /**
     * Holds transaction state for each transaction.
     */
    class TransactionContext
    {
    private:
        timestamp_t start_time_;
        timestamp_t finish_timestamp_;
        bool rollback_;
        storage::BufferPool *buffer_pool_;

    public:
        TransactionContext() = delete;

        TransactionContext(timestamp_t time, timestamp_t finish_timestamp, storage::BufferPool *buffer_pool)
            : start_time_(time), finish_timestamp_(finish_timestamp), rollback_(false), buffer_pool_(buffer_pool) {}

        void Abort();
    };
}