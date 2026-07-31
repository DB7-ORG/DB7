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

        static bool HasConflict(const timestamp_t version_timestamp, const timestamp_t txn_id, const timestamp_t start_time)
        {
            /* Check if there is write-write conflict with another transaction */
            const bool owned_by_other_txn = (!transaction::TransactionUtil::IsCommitted(version_timestamp) && version_timestamp != txn_id);

            /* Check if someone commited after we started */
            const bool newer_committed_version = transaction::TransactionUtil::IsCommitted(version_timestamp) &&
                                                 transaction::TransactionUtil::IsNewerThan(version_timestamp, start_time);

            return owned_by_other_txn || newer_committed_version;
        }
    };
}