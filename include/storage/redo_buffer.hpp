#pragma once

#include "transaction/transaction_common.hpp"
#include "shared/arena/fixed_bump_arena.hpp"
#include "shared/arena/object_pool.hpp"
#include "storage/wal/log_record.hpp"
#include "access/data_chunk.hpp"

#include <vector>

namespace db7::storage
{
    class RedoRecord
    {
    private:
        table_id t_id_;
        page_id p_id_;
        u32 idx_;
        u64 varlen_contents_[0];

    public:
        static u32 GetHeadersSize() { return sizeof(RedoRecord) + sizeof(LogRecord); }

        void *GetDelta() { return varlen_contents_; }

        table_id GetTableId() { return t_id_; }

        page_id GetPageId() { return p_id_; }

        u32 GetRowIndex() { return idx_; }

        static LogRecord *Initialize(byte *const head, const transaction::timestamp_t txn_begin,
                                     access::DataChunkLayout *initializer, table_id t_id, page_id p_id, u32 idx)
        {
            LogRecord *result = LogRecord::InitializeHeader(head, LogRecordType::REDO, initializer->GetTotalSize(), txn_begin);
            auto *body = reinterpret_cast<RedoRecord *>(result->GetDelta());
            body->t_id_ = t_id;
            body->p_id_ = p_id;
            body->idx_ = idx;
            initializer->CreateDataChunk(body->GetDelta());
            return result;
        }
    };

    class RedoBuffer
    {
        using Segment = shared::FixedBumpArena;
        using Pool = shared::ObjectPool<Segment>;

    private:
        // TODO log manager
        void *log_manager_;

        Pool *pool_;

        Segment *segment_old_;

        Segment *segment_;

        byte *last_record_;

        // Flag to denote if this RedoBuffer has flushed records to the log manager already.
        // We use this to determine if we should write an abort record, since we only need to write an abort record if this
        // buffer has previously flushed logs to the log manager. In the case of recovery, the abort record helps it discard
        // changes from aborted txns
        bool has_flushed_;

    public:
        RedoBuffer(void *log_manager, Pool *pool)
            : log_manager_(log_manager), pool_(pool), segment_(nullptr), last_record_(nullptr), has_flushed_(false)
        {
            segment_ = pool_->Get();
            segment_old_ = segment_;
        }

        ~RedoBuffer()
        {
            pool_->Release(segment_);
        }

        bool HasFlushed() const { return has_flushed_; }

        byte *GetLastRecord() const { return last_record_; }

        byte *NewEntry(u32 size, transaction::DurabilityPolicy policy)
        {
            if (!segment_->HasAvailableSpace(size))
            {
                if (log_manager_ != nullptr && policy == transaction::DurabilityPolicy::DISABLED)
                {
                    // TODO flush

                    has_flushed_ = true;
                }
                else
                {
                    segment_ = segment_old_;
                }
            }

            DB7_ASSERT(segment_->HasAvailableSpace(size), "Data larger than page");

            last_record_ = segment_->Allocate(size);

            return last_record_;
        }
    };
}