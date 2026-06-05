#pragma once

#include "storage_common.hpp"

#include <cstring>

namespace db7::storage
{
    struct PageHeader
    {
        u32 count; // this is also offset for varlen storage

        static PageHeader *CastHeader(byte *data) { return reinterpret_cast<PageHeader *>(data); }
    };

    constexpr size_t HEADER_SIZE = sizeof(PageHeader);
    constexpr size_t PAYLOAD_SIZE = PAGE_SIZE - HEADER_SIZE;
}
