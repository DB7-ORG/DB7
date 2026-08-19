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

        DatabaseCatalog *dbc = Builder::CreateDatabaseCatalog(buffer_pool_, disk_mng_, oid);
        databases_map_[oid] = dbc;

        if (!CreateDatabaseEntry(txn, name, dbc->GetDbOid()))
        {
            throw;
        }

        // TODO register abort action in transaction ctx

        (void)bootstrap;

        return oid;
    }

    bool Catalog::CreateDatabaseEntry(transaction::TransactionContext *txn, const std::span<byte> name, db_oid_t oid)
    {
        access::DataChunk *chunk = data_chunk_layout_.CreateDataChunk();

        access::DataChunkBuilder::BuildDatabaseChunk(chunk, oid, name);

        TupleId tup = databases_->Insert(txn, chunk);

        auto res_name = databases_index_datname->InsertUnique(txn, chunk, tup);
        if (!res_name.success)
        {
            return false;
        }

        auto res_oid = databases_index_datoid->InsertUnique(txn, chunk, tup);
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

        shared::VectorValues<TupleId> tids;
        auto result = databases_index_datoid->Get(chunk, tids);
        if (!result.success)
        {
            return false;
        }

        ResultObj<TupleId> res = txn->GetTidForModify(tids.vec, databases_->GetTableOid());
        if (!res.success)
        {
            return false;
        }

        /* INVALID_TID in response means no valid tuple to delete was found */
        TupleId tup_id = res.value;
        if (tup_id == INVALID_TID)
        {
            return true;
        }

        if (!databases_->DeleteUndoRaw(txn, tup_id))
        {
            return false;
        }

        return true;
    }

    bool Catalog::UpdateDatabaseName(transaction::TransactionContext *txn, db_oid_t oid, std::span<byte> name)
    {
        if (!DeleteDatabaseEntry(txn, oid))
        {
            return false;
        }

        if (!CreateDatabaseEntry(txn, name, oid))
        {
            return false;
        }

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