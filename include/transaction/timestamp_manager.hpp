#pragma once

#include "transaction/transaction_common.hpp"
#include "shared/locks/adaptive_version_lock.hpp"
#include "shared/macro_helper.hpp"

#include <atomic>
#include <unordered_set>

namespace db7::transaction
{
    static constexpr timestamp_t INVALID_TXN_TIMESTAMP = timestamp_t(INT64_MIN);
    static constexpr timestamp_t INITIAL_TXN_TIMESTAMP = timestamp_t(0);

    class TimestampManager
    {
    private:
        std::atomic<timestamp_t> time_{INITIAL_TXN_TIMESTAMP};
        std::atomic<timestamp_t> cached_oldest_txn_start_time_{INITIAL_TXN_TIMESTAMP};
        shared::AdaptiveVersionLock lock; // TODO this should be relaxed probably uisng a spinlatch
        std::unordered_set<timestamp_t> curr_running_txns_;

    public:
        TimestampManager() = default;

        ~TimestampManager()
        {
            DB7_ASSERT(curr_running_txns_.empty(),
                       "Destroying the TimestampManager while txns are still running. That seems wrong.");
        }

        /**
         * @return unique timestamp based on current time, and advances one tick
         */
        timestamp_t CheckOutTimestamp() { return time_++; }

        /**
         * @return current time without advancing the tick
         */
        timestamp_t CurrentTime() const { return time_.load(); }

        timestamp_t BeginTransaction();

        void Commit();
    };
}