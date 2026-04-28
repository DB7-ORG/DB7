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
        printf("Page{table_id=%u, page_id=%u, ref=%u, state=%s}\n",
               page.id.tbl_id, page.id.pid, page.ref_count.load(),
               page.state.load() == storage::PageState::VALID ? "VALID" : page.state.load() == storage::PageState::LOADING ? "LOADING"
                                                                      : page.state.load() == storage::PageState::EVICTING  ? "EVICTING"
                                                                                                                           : "UNKNOWN");
    }

    void Print(const storage::BufferPool &pool)
    {
        const storage::Page *pages = pool.GetPagesDebug();
        printf("===========================================================\n");
        for (u32 i = 0; i < (u32)BUFFER_POOL_PAGE_NUM; i++)
        {
            Print(pages[i]);
        }
        printf("===========================================================\n");
    }
}