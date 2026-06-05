#include "catalog/catalog.hpp"
#include "catalog/builder.hpp"
#include "access/projected_rows.hpp"
#include "access/projected_rows_builder.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/varlen_entry.hpp"
#include "shared/align_util.hpp"

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

        DatabaseCatalog *dbc = Builder::CreateDatabaseCatalog(buffer_pool_, disk_mng_);
        databases_map_[oid] = dbc;

        CreateDatabaseEntry(txn, name, dbc);

        // TODO register abort action in transaction ctx
        (void)txn;

        (void)bootstrap;

        return oid;
    }

    bool Catalog::CreateDatabaseEntry(transaction::TransactionContext *txn, const std::span<byte> name, DatabaseCatalog *const dbc)
    {
        db_oid_t oid = dbc->GetDbOid();
        (void)txn;

        // storage::VarlenEntry entry = access::AccessBuilder::CreateVarlenEntry(name, databases_);
        const auto sp = std::span<byte>((byte *)"ssss", 4); // TODO need to create and figure out how to manage varlen entries
        // also refactor varlen entry to store different sizes
        storage::VarlenEntry entry;
        entry.Set(sp);

        const u32 row_count = 1;
        access::DataChunk chunk(CatalogTableColCount::DATABASE, row_count);
        chunk.Set(access::Vector(access::type_id::INTEGER, SizeOf(access::type_id::INTEGER), (byte *)(&oid)));
        chunk.Set(access::Vector(access::type_id::VARCHAR, SizeOf(access::type_id::VARCHAR), (byte *)(&entry)));
        access::TupleId tup = databases_->Insert(chunk);

        databases_index_datoid->Insert(oid, tup.value);

        // TODO fix this
        byte *buf = new byte[name.size() * 16];
        u16 len = access::KeyNormEncoder::Encode(buf, name, false, false, false);
        auto k = access::Key{(u16)name.size(), name.data(), len, buf};
        databases_index_datname->Insert(k, tup.value);

        return true;
    }

    bool Catalog::DeleteDatabase(transaction::TransactionContext *txn, const db_oid_t oid)
    {
        (void)txn;

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

        return true;
    }

    bool Catalog::DeleteDatabaseEntry(transaction::TransactionContext *txn, const db_oid_t oid)
    {
        (void)oid;
        (void)txn;

        // scan the index of the table and get the idx of the row
        u32 idx = 0;
        u32 pid = 1;

        databases_->Delete(idx, pid);

        return true;
    }
}