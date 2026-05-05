#include "access/table.hpp"

#include "storage/fsm/fsm.hpp"
#include "storage/fsm/fsm_varlen.hpp"
#include "shared/align_util.hpp"
#include "storage/page_header.hpp"

namespace db7::access
{

    void Table::Insert(const ProjectedRows &rows)
    {
        u32 page_id = storage::FreeSpaceManager::Get(rows.total_size); // TODO table oid
        (void)page_id;
        storage::PageIdentifier id(oid_, page_id);
        storage::Page *insert_page = buffer_->Pin(id);
        insert_page->WaitIO();

        const auto &map = schema_.GetOffsetMap();
        auto curr = rows.data;

        insert_page->WDataLock();

        auto body = insert_page->GetData();
        storage::PageHeader header(body);
        u32 prev_count = header.IncCount();
        header.WriteHeader(body);

        for (const auto &column : schema_.GetColumns())
        {
            u32 type_size = column.GetTypeSize();
            u32 size = rows.row_count * type_size;

            catalog::col_oid_t oid = column.GetOid();
            u32 offset = map.at(oid) + prev_count * type_size;

            curr = shared::AlignUp(curr, type_size);
            insert_page->WriteOffset(offset, curr, size);
            curr += size;
        }
        insert_page->WDataUnlock();

        PrintPage(insert_page);

        buffer_->Unpin(insert_page, true);
    }

    u32 Table::Insert(std::span<const byte> data)
    {
        u32 page_id = storage::FreeSpaceManagerVarlen::Get(data.size()); // TODO table oid
        (void)page_id;
        storage::PageIdentifier id(varlen_oid_, page_id);
        storage::Page *insert_page = buffer_->Pin(id);
        insert_page->WaitIO();

        insert_page->WDataLock();

        auto body = insert_page->GetData();
        storage::PageHeader header(body);
        u32 offset = header.FetchAddCount(data.size());
        header.WriteHeader(body);

        insert_page->WriteOffset(offset, data.data(), data.size());

        insert_page->WDataUnlock();

        return offset;
    }

    u32 Table::PageCount()
    {
        return disk_mng_->PageCount(oid_);
    }

    void Table::PrintPage(storage::Page *page)
    {
        page->RLock();
        u32 pid = page->GetId().pid;
        u32 tbl = page->GetId().tbl_id;
        page->RUnlock();

        page->WaitIO();

        page->RDataLock(); // read lock

        auto body = page->GetData();
        storage::PageHeader header(body);
        u32 row_count = header.GetCount();

        const auto &map = schema_.GetOffsetMap();

        std::cout << "=== Page Contents ===" << std::endl;
        std::cout << "Row count: " << row_count << std::endl;
        std::cout << "Table id: " << tbl << std::endl;
        std::cout << "Page id: " << pid << std::endl;

        for (u32 i = 0; i < row_count; i++)
        {
            std::cout << "Row " << i << ": ";
            for (const auto &column : schema_.GetColumns())
            {
                u32 type_size = column.GetTypeSize();
                catalog::col_oid_t oid = column.GetOid();
                u32 offset = map.at(oid) + i * type_size;

                const byte *val_ptr = body + offset;

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
                    // for (u32 b = 0; b < type_size; b++)
                    //     printf("%02x", static_cast<unsigned char>(val_ptr[b]));

                    // Print as string, stopping at null or type_size
                    std::cout << std::string(reinterpret_cast<const char *>(val_ptr), strnlen(reinterpret_cast<const char *>(val_ptr), type_size));
                }

                std::cout << " | ";
            }
            std::cout << std::endl;
        }

        std::cout << "=====================" << std::endl;

        page->RDataUnlock();
    }
}