#pragma once

#include "storage_common.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>

namespace db7::storage
{
    struct Page
    {
        page_id pageId;
        byte *data;

        byte *GetOffset(u32 offset)
        {
            DB7_ASSERT(data != nullptr, "Page data in null");
            return data + offset;
        }

        template <typename T>
        void WriteOffset(u32 offset, T value)
        {
            DB7_ASSERT(data != nullptr, "Page data in null");
            memcpy(data + offset, &value, sizeof(T)); // TODO Compiler should optimize this for const fixed types, but test it.
        }

        void WriteOffset(u32 offset, byte *value, u32 size)
        {
            DB7_ASSERT(data != nullptr, "Page data in null");
            memcpy(data + offset, value, size);
        }
    };
}