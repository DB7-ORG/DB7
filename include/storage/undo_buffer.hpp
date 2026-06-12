#pragma once

#include "shared/arena/fixed_bump_arena.hpp"
#include "shared/arena/object_pool.hpp"
#include "transaction/transaction_common.hpp"

#include <vector>
#include <span>

namespace db7::storage
{
    enum class DeltaRecordType : uint8_t
    {
        UPDATE = 0,
        INSERT,
        DELETE,
        INVALID
    };

    class UndoRecord
    {
    private:
        DeltaRecordType type_;
        std::atomic<UndoRecord *> next_;
        std::atomic<transaction::timestamp_t> timestamp_;
        table_id t_id_;
        page_id p_id_;
        u32 idx_;
        u64 varlen_contents_[0];

    public:
        transaction::timestamp_t GetTimestamp()
        {
            return timestamp_.load();
        }

        void Invalidate()
        {
            type_ = DeltaRecordType::INVALID;
        }

        static UndoRecord *InitializeInsert(byte *head, const transaction::timestamp_t timestamp, table_id tbl_id, page_id pid, u32 idx)
        {
            auto *result = reinterpret_cast<UndoRecord *>(head);
            result->type_ = DeltaRecordType::INSERT;
            result->next_ = nullptr;
            result->timestamp_.store(timestamp);
            result->t_id_ = tbl_id;
            result->p_id_ = pid;
            result->idx_ = idx;
            return result;
        }

        static UndoRecord *InitializeDelete(byte *head, const transaction::timestamp_t timestamp, table_id tbl_id, page_id pid, u32 idx)
        {
            auto *result = reinterpret_cast<UndoRecord *>(head);
            result->type_ = DeltaRecordType::DELETE;
            result->next_ = nullptr;
            result->timestamp_.store(timestamp);
            result->t_id_ = tbl_id;
            result->p_id_ = pid;
            result->idx_ = idx;
            return result;
        }

        static UndoRecord *InitializeUpdate(byte *head, const transaction::timestamp_t timestamp, table_id tbl_id, page_id pid, u32 idx, std::span<byte> columns)
        {
            auto *result = reinterpret_cast<UndoRecord *>(head);
            result->type_ = DeltaRecordType::UPDATE;
            result->next_ = nullptr;
            result->timestamp_.store(timestamp);
            result->t_id_ = tbl_id;
            result->p_id_ = pid;
            result->idx_ = idx;
            std::memcpy(result->varlen_contents_, columns.data(), columns.size());
            return result;
        }
    };

    static_assert(sizeof(UndoRecord) % 8 == 0,
                  "a projected row inside the undo record needs to be aligned to 8 bytes"
                  "to ensure true atomicity");

    class UndoBuffer
    {
        using Segment = shared::FixedBumpArena;
        using Pool = shared::ObjectPool<Segment>;

    private:
        Pool *pool_;
        std::vector<Segment *> buffers_;
        byte *last_record_;

    public:
        UndoBuffer(Pool *pool) : pool_(pool), last_record_(nullptr) {}

        ~UndoBuffer()
        {
            for (auto *segment : buffers_)
                pool_->Release(segment);
        }

        byte *LastRecord() const { return last_record_; }

        /**
         * Reserve an undo record with the given size.
         * @param size the size of the undo record to allocate
         * @return a new undo record with at least the given size reserved
         */
        byte *NewEntry(uint32_t size)
        {
            if (buffers_.empty() || !buffers_.back()->HasAvailableSpace(size))
            {
                Segment *new_segment = pool_->Get();
                DB7_ASSERT(reinterpret_cast<uintptr_t>(new_segment) % 8 == 0, "a delta entry should be aligned to 8 bytes");
                buffers_.push_back(new_segment);
            }
            last_record_ = buffers_.back()->Allocate(size);
            return last_record_;
        }
    };
}