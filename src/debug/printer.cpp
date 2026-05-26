#include "debug/printer.hpp"

#include "storage/storage_common.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "access/index/varlen_layout/btree_varlen_layout_leaf.hpp"

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

    void PrintVarlenLayout(byte *data)
    {
        auto *header = reinterpret_cast<access::BtreeHeader<u32> *>(data);
        u32 count = header->count;
        printf("=== Page Dump ===\n");
        printf("count=%-4u  level=%-2u  rlink=%lu  max_val=%u\n",
               header->count, header->level, header->rlink, header->max_val);
        printf("%-6s  %-7s  %-5s  %-10s  %s\n",
               "slot", "offset", "len", "result", "key");
        printf("---------------------------------------------\n");

        /* DANGER does not work if structs change */
        access::Slot *slots = reinterpret_cast<access::Slot *>(data + sizeof(access::BtreeHeader<u32>) + sizeof(access::VarlenHeader));
        for (u32 i = 0; i < count; i++)
        {
            byte *ptr = data + slots[i].offset;
            auto *hdr = reinterpret_cast<access::SlotValHeader<u64> *>(ptr);
            byte *key_data = ptr + sizeof(access::SlotValHeader<u64>);

            printf("[%3u]   %5u    %3u   %10lu  %.*s\n",
                   i, slots[i].offset, hdr->len, hdr->result,
                   hdr->len, (char *)key_data);
        }
        printf("=================\n");
    }
}