#pragma once

#include "storage/storage_common.hpp"
#include "storage/page_header.hpp"

namespace db7::storage
{
    class FreeSpaceManagerVarlen
    {
    private:
        inline static u32 next_page_id = 1;
        inline static u32 free_space = PAGE_SIZE - sizeof(PageHeader); // TODO should be max index

    public:
        static u32 Get(u32 space)
        {
            if (free_space < space)
            {
                next_page_id++;
                free_space = PAGE_SIZE - sizeof(PageHeader);
            }
            free_space -= space;
            return next_page_id;
        }
    };
}