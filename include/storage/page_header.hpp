#pragma once

#include "storage_common.hpp"

#include <cstring>

namespace db7::storage
{
    class PageHeader
    {
    private:
        page_id pid_;
        u32 count_; // this is also offset for varlen storage

    public:
        PageHeader(byte *page_body)
        {
            std::memcpy(this, page_body, sizeof(PageHeader));
        }

        void WriteHeader(byte *page_body)
        {
            std::memcpy(page_body, this, sizeof(PageHeader));
        }

        u32 GetCount() { return count_; }
        u32 IncCount() { return count_++; }
        u32 FetchAddCount(u32 count)
        {
            u32 tmp = count_;
            count_ += count;
            return tmp;
        }
        void SetCount(u32 count) { count_ = count; }
    };

    constexpr size_t HEADER_SIZE = sizeof(PageHeader);
    constexpr size_t PAYLOAD_SIZE = PAGE_SIZE - HEADER_SIZE;
}
