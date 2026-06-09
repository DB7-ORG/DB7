#include "access/table.hpp"

#include "storage/fsm/fsm.hpp"
#include "storage/fsm/fsm_varlen.hpp"
#include "shared/align_util.hpp"
#include "storage/page_header.hpp"
#include "storage/layouts/pax.hpp"

#include <iomanip>

namespace db7::access
{
    TupleId Table::Insert(DataChunk &chunk)
    {
        u32 page_id = storage::FreeSpaceManagerVarlen::Get(chunk.GetTotalSpace()); // TODO table oid
        storage::PageIdentifier id(varlen_oid_, page_id);
        storage::Page *insert_page = buffer_->Pin(id);
        insert_page->WaitIO();

        insert_page->WDataLock();

        byte *body = insert_page->GetData();
        u32 old_count = layout_.IncrementHeaderCount(body, chunk.GetCount());

        for (u32 col_idx = 0; col_idx < schema_.GetColumns().size(); col_idx++)
        {
            layout_.Insert(body, chunk.GetVectorByIdx(col_idx), col_idx, old_count);
        }

        insert_page->WDataUnlock();

        buffer_->Unpin(insert_page, true);

        PrintPage(insert_page);

        /* returns index insede page and page id */
        return {old_count, page_id};
    }

    void Table::Delete(u32 idx, catalog::rel_oid_t pid)
    {
        storage::PageIdentifier id(oid_, pid);
        storage::Page *page = buffer_->Pin(id);
        page->WaitIO();

        page->WDataLock(); // TODO this could be done atomically also
        layout_.Delete(page->GetData(), idx);
        page->WDataUnlock();

        PrintPage(page);
    }

    u32 Table::PageCount()
    {
        return disk_mng_->PageCount(oid_);
    }

    void Table::PrintPage(storage::Page *page)
    {
        page->WLock();
        u32 pid = page->GetId().pid;
        u32 tbl = page->GetId().tbl_id;
        page->WUnlock();

        page->WaitIO();

        page->RDataLock(); // read lock

        auto body = page->GetData();
        auto *header = storage::PageHeader::CastHeader(body);
        u32 row_count = header->count;

        std::cout << "=== Page Contents ===" << std::endl;
        std::cout << "  Row count : " << row_count << std::endl;
        std::cout << "  Table id  : " << tbl << std::endl;
        std::cout << "  Page id   : " << pid << "\n"
                  << std::endl;

        for (u32 i = 0; i < row_count; i++)
        {
            u32 byte_offset = storage::HEADER_SIZE + i / 8;
            bool deleted = (body[byte_offset] >> (i % 8)) & 1;

            std::cout << "  Row " << std::setw(3) << i
                      << (deleted ? "  [DELETED]  " : "             ") << "| ";

            u32 col_idx = 0;
            for (const auto &column : schema_.GetColumns())
            {
                const byte *val_ptr = layout_.Get(body, col_idx++, i);
                u32 type_size = column.GetTypeSize();

                std::cout << column.GetName() << "=";
                if (type_size == 1)
                {
                    std::cout << static_cast<int>(*reinterpret_cast<const int8_t *>(val_ptr));
                }
                else if (type_size == 2)
                {
                    std::cout << *reinterpret_cast<const int16_t *>(val_ptr);
                }
                else if (type_size == 4)
                {
                    std::cout << *reinterpret_cast<const int32_t *>(val_ptr);
                }
                else if (type_size == 8)
                {
                    std::cout << *reinterpret_cast<const int64_t *>(val_ptr);
                }
                else
                {
                    auto entry = *(storage::VarlenEntry *)val_ptr;
                    if (entry.IsInline())
                    {
                        std::cout.write(entry.GetInline(), entry.GetSize());
                    }
                    else
                    {
                        storage::PageIdentifier id(varlen_oid_, entry.GetRef().pid);
                        storage::Page *insert_page = buffer_->Pin(id);
                        insert_page->WaitIO();
                        byte *off = insert_page->GetOffset(entry.GetRef().offset);
                        std::cout.write((char *)off, entry.GetSize());
                    }
                }

                std::cout << std::setw(12) << std::left << /* value */ "" << std::right << "| ";
            }
            std::cout << std::endl;
        }

        std::cout << "=====================" << std::endl;

        page->RDataUnlock();
    }
}