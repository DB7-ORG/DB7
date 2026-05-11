#include "storage/buffer_pool/buffer_pool.hpp"
#include "shared/hash_util.hpp"

#include <thread>
#include <chrono>

namespace db7::storage
{
    BufferPool::BufferPool(DiskScheduler *disk_mng) : disk_mng_(disk_mng)
    {
        pages_ = new Page[BUFFER_POOL_PAGE_NUM];
        auto data = static_cast<u8 *>(std::aligned_alloc(4096, static_cast<size_t>(BUFFER_POOL_PAGE_NUM) * PAGE_SIZE));
        DB7_ASSERT(data != nullptr, "Failed to allocate");
        PageIdentifier id(0);
        for (u32 i = 0; i < BUFFER_POOL_PAGE_NUM; i++)
        {
            pages_[i].WLock();
            pages_[i].SetId(id);
            pages_[i].WUnlock();
            pages_[i].SetData(data + (i * PAGE_SIZE));
        }

        size_t per_partition = BUFFER_POOL_PAGE_NUM / BUFFER_POOL_PARTITION_NUM;
        for (u32 i = 0; i < BUFFER_POOL_PARTITION_NUM; i++)
        {
            partitions_.Reserve(per_partition, i);
        }
    }

    BufferPool::~BufferPool()
    {
        std::free(pages_[0].GetData());
        delete[] pages_;
    }

    Page *BufferPool::GetVictim(PageIdentifier id, u32 &victim_frame_idx, PageIdentifier &victim_page_id)
    {
        u32 max_iters = BUFFER_POOL_PAGE_NUM * 2;

        for (u32 iters = 1; true; iters++)
        {
            u32 head = (sweep_head_++) % BUFFER_POOL_PAGE_NUM;
            Page *page = &pages_[head];
            if (page->TryWLock())
            {
                if (page->IsEvictable())
                {
                    victim_page_id = page->GetId();
                    page->SetId(id);
                    page->Pin();
                    page->SetIOInProgress();
                    page->WUnlock();
                    victim_frame_idx = head;
                    return page;
                }
                page->WUnlock();
            }

            if (iters % max_iters == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        DB7_ASSERT(false, "Unreachable");
        return nullptr;
    }

    void BufferPool::UndoState(Page *victim_page, PageIdentifier victim_page_id)
    {
        victim_page->WLock();
        victim_page->Unpin();
        // if (victim_page->GetId() == wanted)
        victim_page->SetId(victim_page_id);
        victim_page->ClearIOInProgress();
        victim_page->WUnlock();
    }

    bool BufferPool::PageVisit(Page *page, PageIdentifier id)
    {
        page->RLock();
        if (page->GetId() == id)
        { // PageVisit
            page->Pin();
            page->RUnlock();
            return true;
        }
        page->RUnlock();
        return false;
    }

    u32 BufferPool::GetPartitionIdx(PageIdentifier id)
    {
        return shared::HashUtil::murmurhash64(id.packed) % BUFFER_POOL_PARTITION_NUM;
    }

    Page *BufferPool::Pin(PageIdentifier id)
    {
        // LOOKUP
        u32 partIdx = GetPartitionIdx(id);

        while (true)
        {
            Page *page;
            u32 frame_idx = partitions_.Get(id, partIdx);
            if (frame_idx != UINT32_MAX) // page found (fast path)
            {
                page = &pages_[frame_idx];
                if (PageVisit(page, id))
                {
                    return page;
                }
            }

            // FindVictim (slow path)
            u32 victim_frame_idx;
            PageIdentifier victim_page_id;
            page = GetVictim(id, victim_frame_idx, victim_page_id);

            u32 new_frame_idx;
            if (partitions_.Put(id, victim_frame_idx, new_frame_idx, partIdx))
            {
                // TODO FetchPage
                (void)disk_mng_;

                partIdx = GetPartitionIdx(victim_page_id);
                partitions_.Delete(victim_page_id, victim_frame_idx, partIdx);

                IoTask task(IoTask::READ, IoPriority::HIGH, page, id);
                disk_mng_->Enqueue(task);

                return page;
            }

            // UndoState
            UndoState(page, victim_page_id);

            page = &pages_[new_frame_idx];
            if (PageVisit(page, id))
            {
                return page;
            }
            // goto Lookup
        }
    }

    void BufferPool::Unpin(Page *page, bool dirty)
    {
        (void)dirty;
        page->WLock();
        page->Unpin();
        page->WUnlock();
    }

    Page *BufferPool::Reserve(table_id tbl_id, page_id &pid)
    {
        // need to reserve a page from disk manager
        // consider extracting non async components from disk manager
    }
}