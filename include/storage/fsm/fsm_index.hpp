#pragma once

#include "storage/storage_common.hpp"
#include "storage/page_header.hpp"

#include <atomic>

namespace db7::storage
{
    class FreeSpaceManagerIndex
    {
    private:
        inline static std::atomic<u32> next_page_id = 1;

    public:
        static u32 Get()
        {
            return next_page_id++;
        }
    };
}