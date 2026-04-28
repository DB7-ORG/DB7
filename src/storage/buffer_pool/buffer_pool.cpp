#include "storage/buffer_pool/buffer_pool.hpp"
#include "shared/hash_util.hpp"

namespace db7::storage
{
    BufferPool::BufferPool(DiskScheduler *disk_mng) : disk_mng_(disk_mng)
    {
        pages_ = new Page[BUFFER_POOL_PAGE_NUM];
        auto data = static_cast<u8 *>(std::aligned_alloc(4096, BUFFER_POOL_PAGE_NUM * PAGE_SIZE));
        for (u32 i = 0; i < BUFFER_POOL_PAGE_NUM; i++)
        {
            pages_[i].id.packed = 0;
            pages_[i].data = data + (i * PAGE_SIZE);
        }
    }

    BufferPool::~BufferPool()
    {
        std::free(pages_[0].data);
        delete[] pages_;
    }

    Page *BufferPool::GetVictim(PageIdentifier id, u32 &victim_frame_idx, PageIdentifier &victim_page_id)
    {
        u32 max_iters = BUFFER_POOL_PAGE_NUM * 4;
        for (u32 i = 0; i < max_iters; i++)
        {
            u32 head = (sweep_head++) % BUFFER_POOL_PAGE_NUM;
            Page *page = &pages_[head];
            if (page->TryWLock() && page->ref_count == 0)
            {
                victim_page_id = page->id;
                page->id = id;
                page->ref_count++;

                page->io_promise = std::promise<Page *>();
                page->io_future = page->io_future = page->io_promise.get_future().share();
                page->state = PageState::LOADING;

                page->WUnlock();
                victim_frame_idx = head;
                return page;
            }
        }
        DB7_ASSERT(false, "No pages i can evict");
        return nullptr;
    }

    void BufferPool::UndoState(Page *victim_page, PageIdentifier victim_page_id, PageIdentifier wanted)
    {
        victim_page->WLock();
        victim_page->ref_count--;
        if (victim_page->id == wanted)
            victim_page->id = victim_page_id;
        victim_page->WUnlock();
    }

    bool BufferPool::PageVisit(Page *page, PageIdentifier id)
    {
        page->RLock();
        if (page->id.pid == id.pid && page->id.tbl_id == id.tbl_id)
        { // PageVisit
            page->ref_count++;
            page->RUnlock();
            return true;
        }
        page->RUnlock();
        return false;
    }

    std::shared_future<Page *> BufferPool::Pin(PageIdentifier id)
    {
        // LOOKUP
        u32 part = shared::HashUtil::murmurhash64(id.packed) % BUFFER_POOL_PARTITION_NUM;
        BufferPartition *partition = &partitions_[part];

        while (true)
        {
            Page *page;
            u32 frame_idx = partition->Get(id);
            if (frame_idx != UINT32_MAX) // page found (fast path)
            {
                page = &pages_[frame_idx];
                if (PageVisit(page, id))
                {
                    return page->io_future;
                }
            }

            // FindVictim (slow path)
            u32 victim_frame_idx;
            PageIdentifier victim_page_id;
            page = GetVictim(id, victim_frame_idx, victim_page_id);

            u32 new_frame_idx;
            if (partition->Put(id, victim_frame_idx, new_frame_idx))
            {
                // TODO FetchPage
                (void)disk_mng_;

                part = shared::HashUtil::murmurhash64(victim_page_id.packed) % BUFFER_POOL_PARTITION_NUM;
                partition = &partitions_[part];
                partition->Delete(victim_page_id, victim_frame_idx);

                IoTask task(IoTask::READ, IoPriority::HIGH, page, id);
                disk_mng_->Enqueue(task);

                return page->io_future;
            }

            // UndoState
            UndoState(page, victim_page_id, id);

            page = &pages_[new_frame_idx];
            if (PageVisit(page, id))
            {
                return page->io_future;
            }
            // goto Lookup
        }
    }

    void BufferPool::Unpin(Page *page, bool dirty)
    {
        (void)dirty;
        page->RLock();
        page->ref_count--;
        page->RUnlock();
    }
}