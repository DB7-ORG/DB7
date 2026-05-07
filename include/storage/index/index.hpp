#pragma once

namespace db7::storage
{
    class Index
    {
    public:
        virtual ~Index() = default;
        virtual bool Insert(/* ... */) = 0;
        virtual bool Delete(/* ... */) = 0;
        virtual void ScanKey(/* ... */) = 0;
    };
}