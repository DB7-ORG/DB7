#include "access/table.hpp"

#include "storage/fsm/fsm.hpp"
#include "storage/fsm/fsm_varlen.hpp"
#include "shared/align_util.hpp"
#include "storage/page_header.hpp"
#include "storage/layouts/pax.hpp"
#include "transaction/transaction_util.hpp"
#include "access/chunk_utils.hpp"

#include <iomanip>

namespace db7::access
{
    bool HasConflict(transaction::TransactionContext *txn, storage::UndoRecord *version_ptr)
    {
        /* Nobody modified this tuple */
        if (version_ptr == nullptr)
            return false;

        return transaction::TransactionUtil::HasConflict(version_ptr->GetTimestamp(), txn->FinishTime(), txn->StartTime());
    }

    /**
     * @warning make sure to hold the page data lock like w other columns
     */
    bool Table::UpdateUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page, DataChunk *chunk)
    {
        u32 count = layout_.GetMaxRowCount();

        storage::UndoRecord *record = txn->UndoRecordForUpdate(oid_, tup_id.pid, tup_id.index, chunk);

        DataChunk *delta = reinterpret_cast<DataChunk *>(record->GetDelta());

        storage::VersionPtr *versions = txn->GetVersions(page, storage::PageIdentifier{oid_, tup_id.pid}, count);

        storage::UndoRecord *version_ptr;

        do
        {
            version_ptr = versions[tup_id.index].Get();

            if (HasConflict(txn, version_ptr) || layout_.IsDeleted(page->GetData(), tup_id.index))
            {
                record->Invalidate();
                return false;
            }

            ChunkUtils::UpdateSingle(schema_, layout_, chunk, delta, page, tup_id.index);

            record->SetNext(version_ptr);

        } while (!versions[tup_id.index].CompareAndSwap(version_ptr, record));

        return true;
    }

    bool Table::Update(transaction::TransactionContext *txn, u32 idx, DataChunk *chunk)
    {
        u32 page_id = 1;
        storage::PageIdentifier id(oid_, page_id);
        storage::Page *page = buffer_->Pin(id);
        page->WaitIO();

        page->WDataLock();

        bool valid = UpdateUndo(txn, {idx, page_id}, page, chunk);

        page->WDataUnlock();

        page->Unpin();

        return valid;
    }

    /**
     * @warning make sure to hold the page data lock like w other columns
     */
    void Table::InsertUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page)
    {
        u32 count = layout_.GetMaxRowCount();

        storage::UndoRecord *record = txn->UndoRecordForInsert(oid_, tup_id.pid, tup_id.index);

        storage::VersionPtr *versions = txn->GetVersions(page, storage::PageIdentifier{oid_, tup_id.pid}, count);

        versions[tup_id.index].Set(record);
    }

    TupleId Table::Insert(transaction::TransactionContext *txn, DataChunk *chunk)
    {
        // TODO shouldnt use GetSize that seems wastfull
        u32 page_id = storage::FreeSpaceManagerVarlen::Get(chunk->GetSize()); // TODO table oid
        storage::PageIdentifier id(oid_, page_id);
        storage::Page *insert_page = buffer_->Pin(id);
        insert_page->WaitIO();

        insert_page->WDataLock();

        u32 old_count = ChunkUtils::InsertBulk(schema_, layout_, chunk, insert_page);

        TupleId tup_id = {old_count, page_id};

        InsertUndo(txn, tup_id, insert_page);

        insert_page->WDataUnlock();

        // PrintPage(insert_page);

        buffer_->Unpin(insert_page, true);

        /* returns index insede page and page id */
        return tup_id;
    }

    /**
     * @warning make sure to hold the page data lock like w other columns
     */
    bool Table::DeleteUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page)
    {
        u32 count = layout_.GetMaxRowCount();

        storage::UndoRecord *record = txn->UndoRecordForDelete(oid_, tup_id.pid, tup_id.index);

        storage::VersionPtr *versions = txn->GetVersions(page, storage::PageIdentifier{oid_, tup_id.pid}, count);

        storage::UndoRecord *version_ptr;

        do
        {
            version_ptr = versions[tup_id.index].Get();

            if (HasConflict(txn, version_ptr) || layout_.IsDeleted(page->GetData(), tup_id.index))
            {
                record->Invalidate();
                return false;
            }

            record->SetNext(version_ptr);

        } while (!versions[tup_id.index].CompareAndSwap(version_ptr, record));

        return true;
    }

    bool Table::Delete(transaction::TransactionContext *txn, u32 idx, catalog::rel_oid_t pid)
    {
        storage::PageIdentifier id(oid_, pid);
        storage::Page *page = buffer_->Pin(id);
        page->WaitIO();

        page->WDataLock(); // TODO this could be done atomically also or it can use a read lock if i remove a bitmap
        bool is_valid = DeleteUndo(txn, {idx, id.pid}, page);
        if (is_valid)
            layout_.Delete(page->GetData(), idx);
        page->WDataUnlock();

        // PrintPage(page);

        page->Unpin();

        return is_valid;
    }

    u32 Table::PageCount()
    {
        return disk_mng_->PageCount(oid_);
    }

    bool Table::SelectIntoChunk(transaction::TransactionContext *txn, u32 idx, storage::Page *page, DataChunk *chunk)
    {
        ChunkUtils::ReadSingleIntoChunk(schema_, layout_, chunk, page, idx);

        u32 count = layout_.GetMaxRowCount();
        DB7_ASSERT(idx < count, "Out of range index");

        storage::VersionPtr *versions = txn->GetVersions(page, storage::PageIdentifier{oid_, page->GetPageId()}, count);

        storage::UndoRecord *version_ptr = versions[idx].Get();

        bool is_deleted = layout_.IsDeleted(page->GetData(), idx);
        if (version_ptr == nullptr || version_ptr->GetTimestamp() == txn->FinishTime())
        {
            return !is_deleted;
        }

        while (version_ptr != nullptr &&
               transaction::TransactionUtil::IsNewerThan(version_ptr->GetTimestamp(), txn->StartTime()))
        {
            switch (version_ptr->GetType())
            {
            case storage::DeltaRecordType::UPDATE:
            {
                DataChunk *delta = reinterpret_cast<DataChunk *>(version_ptr->GetDelta());
                ChunkUtils::Merge(schema_, chunk, delta);
                break;
            }
            case storage::DeltaRecordType::INSERT:
            case storage::DeltaRecordType::DELETE:
                return false;
            case storage::DeltaRecordType::INVALID:
                break;
            }
            version_ptr = version_ptr->GetNext();
        }

        return !is_deleted;
    }

    bool Table::Select(transaction::TransactionContext *txn, u32 idx, catalog::rel_oid_t pid, DataChunk *chunk)
    {
        storage::PageIdentifier id(oid_, pid);
        storage::Page *page = buffer_->Pin(id);
        page->WaitIO();

        page->RDataLock();
        bool valid = SelectIntoChunk(txn, idx, page, chunk);
        page->RDataUnlock();

        // TODO test
        if (valid)
        {
            chunk->Print(&schema_);
        }
        else
        {
            std::cout << "deleted" << std::endl;
        }

        page->Unpin();

        return valid;
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
            for (const auto &column : schema_)
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