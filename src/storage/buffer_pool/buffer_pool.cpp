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
            pages_[i].pid = 0;
            pages_[i].data = data + (i * PAGE_SIZE);
        }
    }

    BufferPool::~BufferPool()
    {
        std::free(pages_[0].data);
        delete[] pages_;
    }

    Page *BufferPool::GetVictim(page_id wanted, u32 &victim_frame_idx, page_id &victim_page_id)
    {
        u32 max_iters = BUFFER_POOL_PAGE_NUM * 4;
        for (u32 i = 0; i < max_iters; i++)
        {
            u32 head = (sweep_head++) % BUFFER_POOL_PAGE_NUM;
            Page *page = &pages_[head];
            if (page->TryWLock() && page->ref_count == 0)
            {
                victim_page_id = page->pid;
                page->pid = wanted;
                page->ref_count++;

                page->io_promise = std::promise<Page *>();
                page->io_future = page->io_future = page->io_promise.get_future().share();
                page->state = PageState::LOADING;

                page->WUnlock();
                victim_frame_idx = head;
                page->lock.lock();
                return page;
            }
        }
        DB7_ASSERT(false, "No pages i can evict");
        return nullptr;
    }

    void BufferPool::UndoState(Page *victim_page, page_id victim_page_id, page_id wanted)
    {
        victim_page->WLock();
        victim_page->ref_count--;
        if (victim_page->pid == wanted)
            victim_page->pid = victim_page_id;
        victim_page->WUnlock();
    }

    bool BufferPool::PageVisit(Page *page, u32 pid)
    {
        page->RLock();
        if (page->pid == pid)
        { // PageVisit
            page->ref_count++;
            page->RUnlock();
            return true;
        }
        page->RUnlock();
        return false;
    }

    std::shared_future<Page *> BufferPool::Pin(u32 pid)
    {
        // LOOKUP
        u32 part = shared::HashUtil::murmurhash32(pid) % BUFFER_POOL_PARTITION_NUM;
        BufferPartition *partition = &partitions_[part];

        while (true)
        {
            Page *page;
            u32 frame_idx = partition->Get(pid);
            if (frame_idx != UINT32_MAX) // page found (fast path)
            {
                page = &pages_[frame_idx];
                if (PageVisit(page, pid))
                {
                    return page->io_future;
                }
            }

            // FindVictim (slow path)
            u32 victim_frame_idx;
            page_id victim_page_id;
            page = GetVictim(pid, victim_frame_idx, victim_page_id);

            u32 new_frame_idx;
            if (partition->Put(pid, victim_frame_idx, new_frame_idx))
            {
                // TODO FetchPage
                (void)disk_mng_;

                part = shared::HashUtil::murmurhash32(victim_page_id) % BUFFER_POOL_PARTITION_NUM;
                partition = &partitions_[part];
                partition->Delete(victim_page_id, victim_frame_idx);

                IoTask task(IoTask::READ, IoPriority::HIGH, page, pid);
                disk_mng_->Enqueue(task);

                return page->io_future;
            }

            // UndoState
            page->lock.unlock();
            UndoState(page, victim_page_id, pid);

            page = &pages_[new_frame_idx];
            if (PageVisit(page, pid))
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