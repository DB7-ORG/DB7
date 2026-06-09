#include "transaction/timestamp_manager.hpp"

namespace db7::transaction
{
    timestamp_t TimestampManager::BeginTransaction()
    {
        timestamp_t start_time;
        {
            shared::AdaptiveVersionLock::WriteGuard running_guard(lock);

            start_time = time_++;

            const auto ret = curr_running_txns_.emplace(start_time);

            DB7_ASSERT(ret.second, "commit start time should be globally unique");
        } // Release latch on current running transactions
        return start_time;
    }
}