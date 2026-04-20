#include "storage/buffer_pool/buffer_pool.hpp"

namespace db7::storage
{

    Page *BufferPool::Pin(u32 pid)
    {
        auto page = new Page{
            .pageId = pid,
            .data = nullptr};
        return page;
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