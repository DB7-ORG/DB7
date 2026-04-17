#pragma once

#include "storage_common.hpp"

namespace db7::storage
{
    struct Page
    {
        page_id pageId;
        byte *data;
    };
}