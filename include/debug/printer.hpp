#pragma once

#include "common.hpp"

namespace db7::storage
{
    struct PageIdentifier;
    struct Page;
    struct BufferPool;
}

namespace db7::access
{
    class BtreeVarlenLayoutLeaf;
}

namespace db7::shared
{
    void Print(const storage::PageIdentifier &pid);
    void Print(const storage::Page &page);
    void Print(const storage::BufferPool &pool);
    void PrintVarlenLayout(byte *data);
}