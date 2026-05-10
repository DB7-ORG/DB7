#pragma once

#include "storage/index/index.hpp"
#include "storage/storage_common.hpp"

namespace db7::storage
{
    class BTreeIndex : public Index
    {
    public:
        bool Insert(/* ... */) = 0;
        bool Delete(/* ... */) = 0;
        void ScanKey(/* ... */) = 0;
    };
}