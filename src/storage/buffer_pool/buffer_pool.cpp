#include "storage/buffer_pool/buffer_pool.hpp"

namespace db7::storage
{

    Page Pin(u32 pid)
    {
        return Page{
            .pageId = pid,
            .data = nullptr};
    }

    void Unpin(u32 pid, bool dirty)
    {
        (void)pid;
        (void)dirty;
    }

    void Flush(u32 pid)
    {
        (void)pid;
    }
}