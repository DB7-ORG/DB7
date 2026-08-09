#include "catalog/catalog.hpp"
#include "catalog/builder.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/varlen_entry.hpp"
#include "shared/align_util.hpp"
#include "shared/models/tuple_id.hpp"

#include <cstring>

namespace db7::catalog
{
    bool Catalog::RemoveMapping(db_oid_t oid)
    {
        auto it = databases_map_.find(oid);
        if (it == databases_map_.end())
        {
            return false;
        }
        delete it->second;
        databases_map_.erase(it);
        return true;
    }

    db_oid_t Catalog::CreateDatabase(transaction::TransactionContext *txn, const std::span<byte> name, const bool bootstrap)
    {
        db_oid_t oid = next_db_oid_++;

        // TODO register redo event

        DatabaseCatalog *dbc = Builder::CreateDatabaseCatalog(buffer_pool_, disk_mng_);
        databases_map_[oid] = dbc;

        CreateDatabaseEntry(txn, name, dbc); // TODO check return

        // TODO register abort action in transaction ctx

        (void)bootstrap;

        return oid;
    }

    bool Catalog::CreateDatabaseEntry(transaction::TransactionContext *txn, const std::span<byte> name, DatabaseCatalog *const dbc)
    {
        db_oid_t oid = dbc->GetDbOid();

        access::DataChunk *chunk = data_chunk_layout_.CreateDataChunk();

        access::DataChunkBuilder::BuildDatabaseChunk(chunk, oid, name);

        TupleId tup = databases_->Insert(txn, chunk);

        auto res_name = databases_index_datname->Insert(chunk, tup.value);
        if (!res_name.success)
        {
            return false;
        }

        auto res_oid = databases_index_datoid->Insert(chunk, tup.value);
        if (!res_oid.success)
        {
            return false;
        }

        return true;
    }

    bool Catalog::DeleteDatabase(transaction::TransactionContext *txn, const db_oid_t oid)
    {
        if (!DeleteDatabaseEntry(txn, oid))
        {
            DB7_ASSERT(false, "Failed to delete entry");
            return false;
        }

        if (!RemoveMapping(oid))
        {
            DB7_ASSERT(false, "Mapping not found");
            return false;
        }

        // TODO remove all files

        return true;
    }

    bool Catalog::DeleteDatabaseEntry(transaction::TransactionContext *txn, db_oid_t oid)
    {
        auto chunk = data_chunk_layout_.CreateDataChunk();

        access::DataChunkBuilder::BuildDatabaseChunk(chunk, oid, {});

        shared::VectorValues<TupleId> results;
        auto result = databases_index_datoid->Get(chunk, results);
        if (!result.success)
        {
            return false;
        }

        u64 latest_tid;
        for (size_t i = 0; i < results.Size(); i++)
        { // TODO fix index
            if (!txn->ValidateVersion())
            {
                return false;
            }
            latest_tid = results[0];
        }

        TupleId res = {latest_tid};
        if (!databases_->Delete(txn, res.GetIndex(), res.GetPageId()))
        {
            return false;
        }

        return true;
    }

    bool Catalog::UpdateDatabaseName(transaction::TransactionContext *txn, db_oid_t oid, std::span<byte> name)
    {
        access::DataChunk *chunk = data_chunk_layout_.CreateDataChunk();

        access::DataChunkBuilder::BuildDatabaseChunk(chunk, oid, name);

        shared::VectorValues<TupleId> results;
        auto result = databases_index_datoid->Get(chunk, results);
        if (!result.success)
        {
            return false;
        }

        u64 latest_tid;
        for (size_t i = 0; i < results.Size(); i++)
        { // TODO fix index
            if (!txn->ValidateVersion())
            {
                return false;
            }
            latest_tid = results[0];
        }

        TupleId res = {latest_tid};

        databases_->Delete(txn, res.GetIndex(), res.GetPageId());

        databases_->Insert(txn, chunk);

        return true;
    }

    void Catalog::Select(transaction::TransactionContext *txn)
    {
        u32 pid = 1;
        auto chunk = data_chunk_layout_.CreateDataChunk();
        databases_->Select(txn, 0, pid, chunk);
        databases_->Select(txn, 1, pid, chunk);
        databases_->Select(txn, 2, pid, chunk);
        databases_->Select(txn, 3, pid, chunk);
    }
}