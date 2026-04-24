#include "storage/buffer_pool/buffer_pool.hpp"
#include "shared/hash_util.hpp"

namespace db7::storage
{
    BufferPool::BufferPool(DiskManager *disk_mng) : disk_mng_(disk_mng)
    {
        pages_ = new Page[BUFFER_POOL_PAGE_NUM];
        auto data = static_cast<u8 *>(std::aligned_alloc(4096, BUFFER_POOL_PAGE_NUM * PAGE_SIZE));
        for (u32 i = 0; i < BUFFER_POOL_PAGE_NUM; i++)
        {
            pages_[i].page_id = 0;
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
            if (page->TryRLock() && page->ref_count == 0)
            {
                victim_page_id = page->page_id;
                page->page_id = wanted;
                page->ref_count++;
                page->RUnlock();
                victim_frame_idx = head;
                return page;
            }
        }
        DB7_ASSERT(true, "Deadlock in no pages i can evict");
        return nullptr;
    }

    Page *BufferPool::Pin(u32 pid)
    {
        // LOOKUP
        u32 part = shared::HashUtil::murmurhash32(pid) % BUFFER_POOL_PARTITION_NUM;
        BufferPartition *partition = &partitions_[part];
        u32 frame_idx = partition->Get(pid);
        if (frame_idx == UINT32_MAX)
        {
            // find victim
        }
        else
        {
            Page *page = &pages_[frame_idx]; // todo
            page->RLock();
            if (page->page_id == pid)
            {
                page->ref_count++;
                page->RUnlock();
                return page;
            }
            else
            {
                page->RUnlock();

                u32 victim_frame_idx;
                page_id victim_page_id;
                page = GetVictim(pid, victim_frame_idx, victim_page_id);
                if (partition->Put(pid, victim_frame_idx))
                {
                    // FetchPage
                }
                else
                {
                    // UndoState
                }
            }
        }
    }

    void BufferPool::Unpin(u32 pid, bool dirty)
    {
        (void)pid;
        (void)dirty;
    }

    void BufferPool::Flush(u32 pid)
    {
        (void)pid;
    }
}