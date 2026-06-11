#pragma once

#include "transaction/transaction_common.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/undo_buffer.hpp"
#include "storage/storage_common.hpp"
#include "access/data_chunk.hpp"

#include <span>

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
        storage::UndoBuffer undo_buffer_;

    public:
        TransactionContext() = delete;

        TransactionContext(timestamp_t time, timestamp_t finish_time, storage::BufferPool *buffer_pool, shared::ObjectPool<shared::FixedBumpArena> *pool)
            : start_time_(time), finish_time_(finish_time), rollback_(false), buffer_pool_(buffer_pool), undo_buffer_(pool) {}

        timestamp_t StartTime() const { return start_time_; }

        timestamp_t FinishTime() const { return finish_time_; }

        void Abort() { rollback_ = true; };

        bool GetState() { return rollback_; }

        storage::UndoRecord *UndoRecordForInsert(storage::PageIdentifier id, u32 idx)
        {
            byte *result = undo_buffer_.NewEntry(sizeof(storage::UndoRecord));
            return storage::UndoRecord::InitializeInsert(result, finish_time_, id, idx);
        }

        storage::UndoRecord *UndoRecordForDelete(storage::PageIdentifier id, u32 idx)
        {
            byte *result = undo_buffer_.NewEntry(sizeof(storage::UndoRecord));
            return storage::UndoRecord::InitializeDelete(result, finish_time_, id, idx);
        }

        storage::UndoRecord *UndoRecordForUpdate(storage::PageIdentifier id, u32 idx, access::DataChunk *data)
        {
            byte *result = undo_buffer_.NewEntry(sizeof(storage::UndoRecord) + data->GetRowSize());
            return storage::UndoRecord::InitializeUpdate(result, finish_time_, id, idx, data->GetColumnsRaw());
        }
    };
}