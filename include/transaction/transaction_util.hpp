#pragma once

#include "transaction/transaction_common.hpp"

namespace db7::transaction
{
    class TransactionUtil
    {
    public:
        static bool IsCommitted(timestamp_t timestamp) { return static_cast<i64>(timestamp) >= 0; }

        /**
         * Determine if the first timestamp is considered newer than the second.
         * @param a one timestamp
         * @param b other timestamp
         * @return true if a is newer than b, false otherwise
         */
        static bool IsNewerThan(const timestamp_t a, const timestamp_t b)
        {
            return a > b;
        }
    };
}