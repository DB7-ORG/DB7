#pragma once

#include "storage/buffer_pool/buffer_pool.hpp"
#include "access/schema.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "storage/storage_common.hpp"
#include "storage/disk_manager/disk_manager_async.hpp"
#include "storage/varlen_entry.hpp"
#include "access/data_chunk.hpp"
#include "shared/error/exception.hpp"
#include "storage/layouts/pax.hpp"
#include "transaction/transaction_context.hpp"
#include "storage/mvcc/page_version_manager.hpp"

#include <unordered_map>
#include <memory>
#include <span>

/**
 * Layer between storage and other components.
 * Used to implement abstractions that hide whether its working with row store or column store.
 * Idea is for this layer when its working in catalog to act as a row store
 * and when its working in excecution to operate as column store
 */
namespace db7::access
{
    union TupleId
    {
        struct
        {
            u32 index;
            u32 pid;
        };
        u64 value;
    };
    /**
     * Table abstraction
     */
    class Table
    {

    private:
        storage::BufferPool *buffer_;
        storage::DiskManagerAsync *disk_mng_;
        Schema schema_;
        catalog::rel_oid_t oid_;
        catalog::rel_oid_t varlen_oid_;
        storage::PaxLayout layout_;

        static storage::PaxLayout CreateLayoutFromSchema(const Schema &schema)
        {
            std::vector<u16> sizes;
            sizes.reserve(schema.GetCount());
            for (const auto &col : schema)
                sizes.emplace_back(col.GetTypeSize());
            return storage::PaxLayout(std::move(sizes));
        }

        bool UpdateUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page, DataChunk *chunk);

        void InsertUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page);

        bool DeleteUndo(transaction::TransactionContext *txn, TupleId tup_id, storage::Page *page);

        bool SelectIntoChunk(transaction::TransactionContext *txn, u32 idx, storage::Page *page, DataChunk *chunk);

    public:
        DB7_DISALLOW_COPY(Table);

        Table(storage::BufferPool *buffer, storage::DiskManagerAsync *disk_mng, Schema schema, catalog::rel_oid_t oid, catalog::rel_oid_t varlen_oid = 0)
            : buffer_(buffer), disk_mng_(disk_mng), schema_(std::move(schema)), oid_(oid), varlen_oid_(varlen_oid), layout_(CreateLayoutFromSchema(schema_))
        {
            // TODO initialize a table file using disk manager
            if (!disk_mng_->CreateOpenFile(oid_, 1))
            {
                throw IO_EXCEPTION("Could not create/open file");
            }

            if (varlen_oid_ != INVALID_REL_OID && !disk_mng_->CreateOpenFile(varlen_oid_, 1))
            {
                throw IO_EXCEPTION("Could not create/open file");
            }
        }

        bool Update(transaction::TransactionContext *txn, u32 idx, DataChunk *chunk);

        TupleId Insert(transaction::TransactionContext *txn, DataChunk *chunk);

        bool Delete(transaction::TransactionContext *txn, u32 idx, catalog::rel_oid_t pid);

        void Select(transaction::TransactionContext *txn, u32 idx, catalog::rel_oid_t pid, DataChunk *chunk);

        u32 PageCount();

        Schema *GetSchema()
        {
            return &schema_;
        }

        void PrintPage(storage::Page *page);
    };
}