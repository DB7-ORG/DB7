#include "access/table.hpp"

#include "storage/fsm/fsm.hpp"
#include "storage/fsm/fsm_varlen.hpp"
#include "shared/align_util.hpp"
#include "storage/page_header.hpp"

#include <iomanip>

namespace db7::access
{
    void Table::Insert(const ProjectedRows &rows)
    {
        u32 page_id = storage::FreeSpaceManager::Get(rows.total_size); // TODO table oid
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

    std::pair<u32, u32> Table::Insert(std::span<const byte> data)
    {
        u32 page_id = storage::FreeSpaceManagerVarlen::Get(data.size()); // TODO table oid
        storage::PageIdentifier id(varlen_oid_, page_id);
        storage::Page *insert_page = buffer_->Pin(id);
        insert_page->WaitIO();

        insert_page->WDataLock();

        auto body = insert_page->GetData();
        storage::PageHeader header(body);
        u32 offset = header.FetchAddCount(data.size()) + sizeof(header);
        header.WriteHeader(body);

        insert_page->WriteOffset(offset, data.data(), data.size());

        insert_page->WDataUnlock();

        return {offset, page_id};
    }

    void Table::Delete(u32 idx, catalog::rel_oid_t pid)
    {
        storage::PageIdentifier id(oid_, pid);
        storage::Page *page = buffer_->Pin(id);
        page->WaitIO();

        u32 byte_offset = storage::HEADER_SIZE + idx / 8;
        byte mask = byte(1 << (idx % 8));

        page->WDataLock();
        byte current = *page->GetOffset(byte_offset);
        page->WriteOffset(byte_offset, byte(current | mask));
        // TODO change mvcc headers also
        page->WDataUnlock();
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
        storage::PageHeader header(body);
        u32 row_count = header.GetCount();

        const auto &map = schema_.GetOffsetMap();

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