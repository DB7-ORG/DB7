#include "catalog/catalog.hpp"
#include "catalog/builder.hpp"
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

        // TODO register redo event

        DatabaseCatalog *dbc = Builder::CreateDatabaseCatalog(buffer_pool_, disk_mng_);
        databases_map_[oid] = dbc;

        CreateDatabaseEntry(txn, name, dbc);

        // TODO register abort action in transaction ctx

        (void)bootstrap;

        return oid;
    }

    bool Catalog::CreateDatabaseEntry(transaction::TransactionContext *txn, const std::span<byte> name, DatabaseCatalog *const dbc)
    {
        db_oid_t oid = dbc->GetDbOid();

        // TODO figure out what to do w varlen
        storage::VarlenEntry entry;
        entry.Set(name);

        access::DataChunk *chunk = data_chunk_layout_.CreateDataChunk();
        auto iter = chunk->InitIterator();
        iter.PushBack(oid);
        iter.PushBack(entry);
        access::TupleId tup = databases_->Insert(txn, chunk);

        // TODO fix this
        // TODO fix index
        // byte *buf = new byte[name.size() * 16];
        // u16 len = access::KeyNormEncoder::Encode(buf, name, false, false, false);
        // auto k = access::Key{(u16)name.size(), name.data(), len, buf};
        // databases_index_datname->Insert(k, tup.value); // TODO validate no duplicate error

        // byte *buf2 = new byte[sizeof(db_oid_t) * 4];
        // u32 len2 = access::KeyNormEncoder::Encode(buf2, oid, false, false, false);
        // auto k2 = access::Key{(u16)sizeof(db_oid_t), reinterpret_cast<byte *>(&oid), (u16)len2, buf2};
        // databases_index_datoid->Insert(k2, tup.value);

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
        // byte *buf = new byte[sizeof(db_oid_t) * 4];
        // u32 len = access::KeyNormEncoder::Encode(buf, oid, false, false, false);
        // auto k = access::Key{(u16)sizeof(db_oid_t), reinterpret_cast<byte *>(&oid), (u16)len, buf};     // TODO fix index
        // auto k = access::Key{};
        // auto result = databases_index_datoid->Get(k);
        // if (!result.success)
        // {
        //     return false;
        // }
        // access::TupleId res{.value = result.value};
        // u32 idx = res.index;
        // u32 pid = res.pid;

        // auto chunk = data_chunk_layout_.CreateDataChunk();
        // if (!databases_->Select(txn, res.index, res.pid, chunk))
        // {
        //     return false;
        // }
        // auto name = *reinterpret_cast<storage::VarlenEntry *>(
        //     chunk->Get(catalog::col_oid_t(CatalogColumnOid::DATNAME))); // TODO get by index is better
        // (void)name;

        // if (!databases_->Delete(txn, idx, pid))
        // {
        //     return false;
        // }

        // if (!databases_index_datoid->Delete(txn, oid))
        // {
        //     return false;
        // }

        // if (!databases_index_datname->Delete(txn, name))
        // {
        //     return false;
        // }

        return true;
    }

    bool Catalog::UpdateDatabaseName(transaction::TransactionContext *txn, db_oid_t oid, std::span<char> name)
    {
        // byte *buf = new byte[sizeof(db_oid_t) * 16];
        // u32 len = access::KeyNormEncoder::Encode(buf, oid, false, false, false);
        // auto k = access::Key{(u16)sizeof(db_oid_t), reinterpret_cast<byte *>(&oid), (u16)len, buf};     // TODO fix index
        // auto k = access::Key{};
        // auto result = databases_index_datoid->Get(k);
        // if (!result.success)
        // {
        //     return false;
        // }
        // access::TupleId res{.value = result.value};
        // u32 idx = res.index;

        // storage::VarlenEntry entry;
        // entry.Set(name);

        // access::DataChunk *chunk = data_chunk_layout_.CreateDataChunk();
        // auto iter = chunk->InitIterator();
        // iter.PushBack(oid);
        // iter.PushBack(entry);
        // return databases_->Update(txn, idx, chunk);
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