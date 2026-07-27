#pragma once

#include "storage/storage_common.hpp"
#include "transaction/transaction_common.hpp"

namespace db7::storage
{
    enum LogRecordType : u8
    {
        REDO = 0,
        DELETE,
        COMMIT,
        ABORT
    };

    class LogRecord
    {
    private:
        LogRecordType type_;
        u32 size_;
        transaction::timestamp_t txn_begin_;
        u64 varlen_contents_[0];

    public:
        LogRecordType RecordType() const { return type_; }

        u32 Size() const { return size_; }

        transaction::timestamp_t TxnBegin() const { return txn_begin_; }

        void *GetDelta() { return varlen_contents_; }

        static LogRecord *InitializeHeader(byte *const head, const LogRecordType type, const u32 size,
                                           const transaction::timestamp_t txn_begin)
        {
            auto *result = reinterpret_cast<LogRecord *>(head);
            result->type_ = type;
            result->size_ = size;
            result->txn_begin_ = txn_begin;
            return result;
        }
    };
}