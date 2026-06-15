#include "access/table.hpp"

#include "storage/fsm/fsm.hpp"
#include "storage/fsm/fsm_varlen.hpp"
#include "shared/align_util.hpp"
#include "storage/page_header.hpp"
#include "storage/layouts/pax.hpp"
#include "transaction/transaction_util.hpp"

#include <iomanip>

namespace db7::access
{
    bool HasConflict(transaction::TransactionContext *txn, storage::UndoRecord *version_ptr)
    {
        /* Nobody modified this tuple */
        if (version_ptr == nullptr)
            return false;

        const transaction::timestamp_t version_timestamp = version_ptr->GetTimestamp();
        const transaction::timestamp_t txn_id = txn->FinishTime();
        const transaction::timestamp_t start_time = txn->StartTime();

        /* Check if there is write-write conflict with another transaction */
        const bool owned_by_other_txn = (!transaction::TransactionUtil::IsCommitted(version_timestamp) && version_timestamp != txn_id);

        /* Check if someone commited after we started */
        const bool newer_committed_version = transaction::TransactionUtil::IsCommitted(version_timestamp) &&
                                             transaction::TransactionUtil::IsNewerThan(version_timestamp, start_time);

        return owned_by_other_txn || newer_committed_version;
    }

    /**
     * @warning make sure to hold the page data lock like w other columns
     */
    bool Table::UpdateUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page, DataChunk &chunk)
    {
        u32 count = layout_.GetRowCount();

        storage::UndoRecord *record = txn->UndoRecordForUpdate(oid_, tup_id.pid, tup_id.index, chunk);

        storage::VersionPtr *versions = txn->GetVersions(page, storage::PageIdentifier{oid_, tup_id.pid}, count);

        storage::UndoRecord *version_ptr;

        do
        {
            version_ptr = versions[tup_id.index].Get();

            if (HasConflict(txn, version_ptr))
            {
                record->Invalidate();
                return false;
            }

            std::vector<u32> indexes = schema_.GetColumnIndexes(chunk.GetColumnIds()); // TODO Move this outside of the api
            for (u32 i = 0; i < indexes.size(); i++)
            {
                u32 size = schema_.GetColumn(indexes[i]).GetTypeSize();
                byte *ptr = layout_.Get(page->GetData(), indexes[i], tup_id.index); // TODO need to copy current values and insert to delta store
                layout_.Update(ptr, std::span<byte>(chunk.Access(indexes[i]), size));
            }

            record->SetNext(version_ptr);

        } while (versions[tup_id.index].CompareAndSwap(version_ptr, record));

        return true;
    }

    TupleId Table::Update(transaction::TransactionContext *txn, DataChunk &chunk)
    {
    }

    /**
     * @warning make sure to hold the page data lock like w other columns
     */
    void Table::InsertUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page)
    {
        u32 count = layout_.GetRowCount();

        storage::UndoRecord *record = txn->UndoRecordForInsert(oid_, tup_id.pid, tup_id.index);

        storage::VersionPtr *versions = txn->GetVersions(page, storage::PageIdentifier{oid_, tup_id.pid}, count);

        versions[tup_id.index].Set(record);
    }

    TupleId Table::Insert(transaction::TransactionContext *txn, DataChunk &chunk)
    {
        u32 page_id = storage::FreeSpaceManagerVarlen::Get(chunk.GetSize()); // TODO table oid
        storage::PageIdentifier id(varlen_oid_, page_id);
        storage::Page *insert_page = buffer_->Pin(id);
        insert_page->WaitIO();

        insert_page->WDataLock();

        byte *body = insert_page->GetData();
        u32 old_count = layout_.IncrementHeaderCount(body, 1);

        for (auto &col : schema_.GetColumns())
        {
            layout_.Insert(body, std::span<byte>(chunk.Access(col.GetPosiiton()), col.GetTypeSize()), col.GetPosiiton(), old_count);
        }

        TupleId tup_id = {old_count, page_id};

        InsertUndo(txn, tup_id, insert_page);

        insert_page->WDataUnlock();

        PrintPage(insert_page);

        buffer_->Unpin(insert_page, true);

        /* returns index insede page and page id */
        return tup_id;
    }

    /**
     * @warning make sure to hold the page data lock like w other columns
     */
    bool Table::DeleteUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page)
    {
        u32 count = layout_.GetRowCount();

        storage::UndoRecord *record = txn->UndoRecordForDelete(oid_, tup_id.pid, tup_id.index);

        storage::VersionPtr *versions = txn->GetVersions(page, storage::PageIdentifier{oid_, tup_id.pid}, count);

        storage::UndoRecord *version_ptr;

        do
        {
            version_ptr = versions[tup_id.index].Get();

            if (HasConflict(txn, version_ptr))
            {
                record->Invalidate();
                return false;
            }

            record->SetNext(version_ptr);

        } while (versions[tup_id.index].CompareAndSwap(version_ptr, record));

        return true;
    }

    bool Table::Delete(transaction::TransactionContext *txn, u32 idx, catalog::rel_oid_t pid)
    {
        storage::PageIdentifier id(oid_, pid);
        storage::Page *page = buffer_->Pin(id);
        page->WaitIO();

        page->WDataLock(); // TODO this could be done atomically also
        bool is_valid = DeleteUndo(txn, {idx, id.pid}, page);
        layout_.Delete(page->GetData(), idx);
        page->WDataUnlock();

        PrintPage(page);

        page->Unpin();

        return is_valid;
    }

    u32 Table::PageCount()
    {
        return disk_mng_->PageCount(oid_);
    }

    void Table::ScanIntoChunk(transaction::TransactionContext *txn, u32 idx, storage::Page *page, DataChunk &chunk)
    {
        byte *data = page->GetData();

        std::vector<u32> indexes = schema_.GetColumnIndexes(chunk.GetColumnIds());

        for (auto &column_idx : indexes)
        {
            u32 size = schema_.GetColumn(column_idx).GetPosiiton();
            byte *ptr = layout_.Get(data, column_idx, idx);
            chunk.PushBack(std::span<byte>(ptr, size));
        }
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