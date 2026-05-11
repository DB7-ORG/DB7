#pragma once

#include "common.hpp"

namespace db7::storage
{
    class Index
    {
    public:
        virtual ~Index() = default;
        virtual bool Insert(u64 key, u64 value) = 0;
        virtual bool Delete(/* ... */) = 0;
        virtual void ScanKey(/* ... */) = 0;
    };
}