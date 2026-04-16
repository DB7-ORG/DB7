#pragma once

#include "common.hpp"

namespace db7::storage
{
    struct Page
    {
        u32 pageId;
        byte *data;
    };
}