#include "debug/printer.hpp"

#include "storage/storage_common.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"

namespace db7::shared
{
    void Print(const storage::PageIdentifier &pid)
    {
        printf("PageId{tbl=%u, page=%u}\n", pid.tbl_id, pid.pid);
    }

    void Print(const storage::Page &page)
    {
        printf("Page{table_id=%-4u  page_id=%-6u  ref=%-3u  io_in_progress=%s}\n",
               page.GetId().tbl_id,
               page.GetId().pid,
               page.PinCount(),
               page.IsIOInProgress() ? "true" : "false");
    }

    void Print(const storage::BufferPool &pool)
    {
        const storage::Page *pages = pool.GetPagesDebug();
        printf("===========================================================\n");
        printf("%-10s %-8s %-5s %s\n", "table_id", "page_id", "ref", "io_in_progress");
        printf("-----------------------------------------------------------\n");
        for (u32 i = 0; i < (u32)BUFFER_POOL_PAGE_NUM; i++)
        {
            Print(pages[i]);
        }
        printf("===========================================================\n");
    }
}