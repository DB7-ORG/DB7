#pragma once

#include "transaction/timestamp_manager.hpp"

#include <memory>

namespace db7::transaction
{
    class DeferredActionManager
    {
    private:
        TimestampManager *timestamp_manager_;

    public:
        DeferredActionManager(TimestampManager *timestamp_manager)
            : timestamp_manager_(timestamp_manager) {}
    };
}