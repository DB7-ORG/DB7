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
    };
}