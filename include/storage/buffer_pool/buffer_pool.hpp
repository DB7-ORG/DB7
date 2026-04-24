#pragma once

#include "common.hpp"
#include "storage/page.hpp"
#include "storage/disk_manager/disk_manager.hpp"
#include "storage/buffer_pool/buffer_partition.hpp"

#include <atomic>

namespace db7::storage
{
    class BufferPool
    {
    private:
        DiskManager *disk_mng_;
        Page *pages_;
        u32 poolSize_;
        std::atomic<u32> sweep_head;
        BufferPartition partitions_[BUFFER_POOL_PARTITION_NUM];

        Page *GetVictim(page_id wanted, u32 &victim_frame_idx, page_id &victim_page_id);

    public:
        BufferPool(DiskManager *disk_mng);
        ~BufferPool();

        Page *Pin(u32 pid); // TODO all of these should have private methods calling DiskManager
        void Unpin(u32 pid, bool dirty);
        void Flush(u32 pid);
    };
}