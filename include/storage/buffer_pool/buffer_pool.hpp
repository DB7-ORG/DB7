#pragma once

#include "common.hpp"
#include "storage/page.hpp"

namespace db7::storage
{

    class BufferPool
    {
    private:
        using DiskManager = u64;
        DiskManager *diskMng;

    public:
        BufferPool() {}
        Page *Pin(u32 pid); // TODO all of these should have private methods calling DiskManager
        void Unpin(u32 pid, bool dirty);
        void Flush(u32 pid);
    };
}