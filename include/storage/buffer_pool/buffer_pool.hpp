#pragma once

#include "common.hpp"
#include "storage/page.hpp"
#include "storage/disk_manager/disk_manager.hpp"

namespace db7::storage
{

    class BufferPool
    {
    private:
        DiskManager *disk_mng_;
        Page *pages_;
        u32 poolSize_;

    public:
        BufferPool(DiskManager *disk_mng);
        Page *Pin(u32 pid); // TODO all of these should have private methods calling DiskManager
        void Unpin(u32 pid, bool dirty);
        void Flush(u32 pid);
    };
}