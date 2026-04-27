#pragma once

#include "storage/storage_common.hpp"

#define TABLE_FILE_HEADER_SIZE 16

namespace db7::storage
{
    struct PACKED TableFileHeader
    {
        u32 magic;
        table_id tbl_id;
        u32 page_count;     /* includes the header page itself */
        u32 free_page_head; /* page_id of first free page, 0 = none */
        char reserved[PAGE_SIZE - TABLE_FILE_HEADER_SIZE];
    };
}