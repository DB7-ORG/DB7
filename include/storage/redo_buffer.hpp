#pragma once

#include "transaction/transaction_common.hpp"
#include "shared/arena/fixed_bump_arena.hpp"
#include "shared/arena/object_pool.hpp"

#include <vector>

namespace db7::storage
{
    class RedoRecord
    {
    private:
    public:
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