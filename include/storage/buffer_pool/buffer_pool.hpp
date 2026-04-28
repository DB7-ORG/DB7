#pragma once

#include "common.hpp"
#include "storage/page.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"
#include "storage/buffer_pool/buffer_partition.hpp"

#include <atomic>

namespace db7::storage
{
    class BufferPool
    {
    private:
        DiskScheduler *disk_mng_;
        Page *pages_;
        u32 poolSize_;
        std::atomic<u32> sweep_head;
        BufferPartition partitions_[BUFFER_POOL_PARTITION_NUM];

        Page *GetVictim(page_id wanted, u32 &victim_frame_idx, page_id &victim_page_id);
        void UndoState(Page *victim_page, page_id victim_page_id, page_id wanted);
        bool PageVisit(Page *page, u32 pid);

    public:
        BufferPool(DiskScheduler *disk_mng);
        ~BufferPool();

        std::shared_future<Page *> Pin(u32 pid);
        void Unpin(Page *page, bool dirty = false);
    };
}