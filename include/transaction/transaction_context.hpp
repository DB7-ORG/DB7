#pragma once

#include "transaction/transaction_common.hpp"

namespace db7::transaction
{

    /**
     * Holds transaction state for each transaction.
     */
    class TransactionContext
    {
    private:
        timestamp_t start_time_;
        bool rollback_;

    public:
        TransactionContext() = delete;
        TransactionContext(timestamp_t time)
            : start_time_(time), rollback_(false) {}

        void Abort();
    };
}