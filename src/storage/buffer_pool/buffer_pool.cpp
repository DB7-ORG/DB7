#include "storage/buffer_pool/buffer_pool.hpp"

namespace db7::storage
{

    Page BufferPool::Pin(u32 pid)
    {
        return Page{
            .pageId = pid,
            .data = nullptr};
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