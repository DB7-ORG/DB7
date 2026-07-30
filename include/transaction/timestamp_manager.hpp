#pragma once

#include "transaction/transaction_common.hpp"

#include <atomic>

namespace db7::transaction
{
    static constexpr timestamp_t INVALID_TXN_TIMESTAMP = timestamp_t(INT64_MIN);
    static constexpr timestamp_t INITIAL_TXN_TIMESTAMP = timestamp_t(0);

    class TimestampManager
    {
    private:
        std::atomic<timestamp_t> time_{INITIAL_TXN_TIMESTAMP};

    public:
        TimestampManager() = default;

        ~TimestampManager() {}

        /**
         * @return unique timestamp based on current time, and advances one tick
         */
        timestamp_t CheckOutTimestamp() { return time_++; }

        /**
         * @return current time without advancing the tick
         */
        timestamp_t CurrentTime() const { return time_.load(); }

        timestamp_t BeginTransaction() { return CheckOutTimestamp(); }
    };
}