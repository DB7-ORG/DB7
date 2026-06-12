#pragma once

#include "common.hpp"
#include "storage/page.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"
#include "storage/buffer_pool/buffer_partitions.hpp"
#include "storage/mvcc/mapping_table_manager.hpp"

#include <atomic>

namespace db7::storage
{
    class BufferPool
    {
    private:
        DiskScheduler *disk_mng_;
        Page *pages_;
        u32 pool_size_;
        std::atomic<u32> sweep_head_;
        BufferPartitions partitions_;
        MappingTableManager *version_table_;

        Page *GetVictim(PageIdentifier id, u32 &victim_frame_idx, PageIdentifier &victim_page_id, bool isIO = true);
        void UndoState(Page *victim_page, PageIdentifier victim_page_id);
        bool PageVisit(Page *page, PageIdentifier id);
        u32 GetPartitionIdx(PageIdentifier id);

    public:
        BufferPool(DiskScheduler *disk_mng, MappingTableManager *version_table);
        ~BufferPool();

        Page *Pin(PageIdentifier id);
        void Unpin(Page *page, bool dirty = false);
        Page *Reserve(table_id tbl_id);

        // TODO remove
        Page *GetPagesDebug() const
        {
            return pages_;
        };
    };
}