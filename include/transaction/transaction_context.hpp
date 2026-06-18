#pragma once

#include "transaction/transaction_common.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/undo_buffer.hpp"
#include "storage/storage_common.hpp"
#include "access/data_chunk.hpp"
#include "storage/redo_buffer.hpp"

#include <span>
#include <forward_list>

namespace db7::transaction
{

    /**
     * Holds transaction state for each transaction.
     */
    class TransactionContext
    {
    private:
        timestamp_t start_time_;
        timestamp_t finish_time_;
        bool rollback_;
        storage::BufferPool *buffer_pool_;
        storage::PageVersionManager *version_manager_;

        storage::UndoBuffer undo_buffer_;
        storage::RedoBuffer redo_buffer_;

        // std::forward_list<TransactionEndAction> abort_actions_;
        // std::forward_list<TransactionEndAction> commit_actions_;

    public:
        TransactionContext() = delete;

        TransactionContext(
            timestamp_t time,
            timestamp_t finish_time,
            storage::BufferPool *buffer_pool,
            storage::PageVersionManager *version_manager,
            shared::ObjectPool<shared::FixedBumpArena> *pool)
            : start_time_(time),
              finish_time_(finish_time),
              rollback_(false),
              buffer_pool_(buffer_pool),
              version_manager_(version_manager),
              undo_buffer_(pool),
              redo_buffer_(nullptr, pool) {} // TODO

        timestamp_t StartTime() const { return start_time_; }

        timestamp_t FinishTime() const { return finish_time_; }

        void Abort() { rollback_ = true; };

        bool GetState() { return rollback_; }

        /**
         * @warning make sure to hold the page data lock like w other columns
         */
        storage::VersionPtr *GetVersions(storage::Page *page, storage::PageIdentifier id, u32 count)
        {
            storage::VersionPtr *versions_arr = page->GetVersions();

            if (versions_arr == nullptr)
            {
                auto *new_version = version_manager_->InitializeVersions(id, count);

                page->SetVersions(new_version);

                return new_version;
            }

            return versions_arr;
        }

        storage::UndoRecord *UndoRecordForInsert(table_id tbl_id, page_id pid, u32 idx)
        {
            byte *result = undo_buffer_.NewEntry(sizeof(storage::UndoRecord));
            return storage::UndoRecord::InitializeInsert(result, finish_time_, tbl_id, pid, idx);
        }

        storage::UndoRecord *UndoRecordForDelete(table_id tbl_id, page_id pid, u32 idx)
        {
            byte *result = undo_buffer_.NewEntry(sizeof(storage::UndoRecord));
            return storage::UndoRecord::InitializeDelete(result, finish_time_, tbl_id, pid, idx);
        }

        storage::UndoRecord *UndoRecordForUpdate(table_id tbl_id, page_id pid, u32 idx, access::DataChunk *chunk)
        {
            byte *result = undo_buffer_.NewEntry(sizeof(storage::UndoRecord) + chunk->GetSize());
            return storage::UndoRecord::InitializeUpdate(result, finish_time_, tbl_id, pid, idx, chunk->GetHeaderPtr());
        }
    };
}