#include "catalog/database_catalog.hpp"
#include "transaction/transaction_util.hpp"
#include "shared/models/tuple_id.hpp"

namespace db7::catalog
{

    bool DatabaseCatalog::TryLock(transaction::TransactionContext *txn)
    {
        auto current_val = write_lock_.load();
        if (current_val == txn->FinishTime())
        {
            return true;
        }

        if (transaction::TransactionUtil::HasConflict(current_val, txn->FinishTime(), txn->StartTime()))
        {
            txn->Abort();
            return false;
        }

        if (!write_lock_.compare_exchange_strong(current_val, txn->FinishTime()))
        {
            txn->Abort();
            return false;
        }

        // TODO register abort and commit actions here
        return true;
    }

    ResultObj<namespace_oid_t> DatabaseCatalog::CreateNamespaceEntry(transaction::TransactionContext *txn, const std::span<byte> name, namespace_oid_t oid)
    {
        access::DataChunk *chunk = namespace_data_chunk_layout_->CreateDataChunk();

        access::DataChunkBuilder::BuildNamespaceChunk(chunk, oid, name);

        TupleId tup = namespaces_->Insert(txn, chunk);

        auto res_name = namespaces_index_nspname_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_name.success)
        {
            return ResultObj<namespace_oid_t>::Fail("Failed to insert to nspname index");
        }

        auto res_oid = namespaces_index_nspoid_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_oid.success)
        {
            return ResultObj<namespace_oid_t>::Fail("Failed to insert to nspoid index");
        }

        return ResultObj<namespace_oid_t>(oid);
    }

    ResultObj<namespace_oid_t> DatabaseCatalog::CreateNamespace(transaction::TransactionContext *txn, const std::span<byte> name)
    {
        if (!TryLock(txn))
            return ResultObj<namespace_oid_t>::Fail("Couldnt aquire a lock");
        auto oid = next_namespace_oid_++;
        return CreateNamespaceEntry(txn, name, oid);
    }

    bool DatabaseCatalog::DeleteNamespaceEntry(transaction::TransactionContext *txn, namespace_oid_t oid)
    {
        auto chunk = namespace_data_chunk_layout_->CreateDataChunk();

        access::DataChunkBuilder::BuildNamespaceChunk(chunk, oid, {});

        shared::VectorValues<TupleId> tids;
        auto result = namespaces_index_nspoid_->Get(chunk, tids);
        if (!result.success)
        {
            return false;
        }

        ResultObj<TupleId> res = txn->GetTidForModify(tids.vec, namespaces_->GetTableOid());
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

        if (!namespaces_->DeleteUndoRaw(txn, tup_id))
        {
            return false;
        }

        return true;
    }

    bool DatabaseCatalog::DeleteNamespace(transaction::TransactionContext *txn, namespace_oid_t oid)
    {
        if (!TryLock(txn))
            return false;
        if (!DeleteNamespaceEntry(txn, oid))
        {
            return false;
        }
        // TODO need to implement cascading deletes for all objects in namespace
        return true;
    }

    ResultObj<void> DatabaseCatalog::ExistsNamespace(transaction::TransactionContext *txn, namespace_oid_t oid)
    {
        auto *ns_chunk = namespace_data_chunk_layout_->CreateDataChunk();

        access::DataChunkBuilder::BuildNamespaceChunk(ns_chunk, oid);

        shared::VectorValues<TupleId> results;
        auto ns_res = namespaces_index_nspoid_->Get(ns_chunk, results);
        if (!ns_res.success)
        {
            return ResultObj<void>::Fail("Namespace does not exist");
        }

        if (!txn->GetTidExists(results.vec, namespaces_->GetTableOid()))
        {
            return ResultObj<void>::Fail("Namespace does not exist");
        }

        return ResultObj<void>::Ok();
    }

    bool DatabaseCatalog::UpdateNamespaceName(transaction::TransactionContext *txn, namespace_oid_t oid, std::span<byte> name)
    {
        if (!DeleteNamespaceEntry(txn, oid))
        {
            return false;
        }

        auto res = CreateNamespaceEntry(txn, name, oid);
        if (!res.success)
        {
            return false;
        }

        return true;
    }

    ResultObj<attribute_oid_t> DatabaseCatalog::CreateColumnEntry(transaction::TransactionContext *txn, class_oid_t rel_oid, access::SchemaColumn &column)
    {
        access::DataChunk *chunk = attribute_data_chunk_layout_->CreateDataChunk();

        attribute_oid_t oid = next_attribute_oid_++;

        access::DataChunkBuilder::BuildAttributeChunk(chunk, oid, rel_oid, column.GetNameSpan(), column.GetType(), column.GetTypeSize(), column.IsNullable());

        TupleId tup = attributes_->Insert(txn, chunk);

        auto res_name = attributes_index_attrelid_attname_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_name.success)
        {
            return ResultObj<attribute_oid_t>::Fail("Failed to insert to attrelid_attname index");
        }

        auto res_oid = attributes_index_attnum_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_oid.success)
        {
            return ResultObj<attribute_oid_t>::Fail("Failed to insert to attnum index");
        }

        return ResultObj<attribute_oid_t>(oid);
    }

    ResultObj<class_oid_t> DatabaseCatalog::CreateTableEntry(transaction::TransactionContext *txn, const std::span<byte> name, class_oid_t oid, namespace_oid_t namespace_oid)
    {
        access::DataChunk *chunk = classes_data_chunk_layout_->CreateDataChunk();

        // TODO reloptions needs to be null. need to add that to chunk
        access::DataChunkBuilder::BuildClassChunk(chunk, oid, name, namespace_oid, ToChar(RelKind::REGULAR_TABLE), name);

        TupleId tup = classes_->Insert(txn, chunk);

        auto res_relname = classes_index_relname_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_relname.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to relname index");
        }

        auto res_name = classes_index_relnamespace_->Insert(chunk, tup.GetValue());
        if (!res_name.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to relnamespace index");
        }

        auto res_oid = classes_index_reloid_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_oid.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to reloid index");
        }

        return ResultObj<class_oid_t>(oid);
    }

    ResultObj<class_oid_t> DatabaseCatalog::CreateTable(transaction::TransactionContext *txn, const std::span<byte> name, namespace_oid_t namespace_oid, access::Schema &schema)
    {
        if (!TryLock(txn))
            return INVALID_OID;

        auto ns_res = ExistsNamespace(txn, namespace_oid);
        if (!ns_res.success)
        {
            return ResultObj<class_oid_t>::Fail(ns_res.message);
        }

        auto oid = next_class_oid_++;

        auto res = CreateTableEntry(txn, name, oid, namespace_oid);
        if (!res.success)
        {
            return res;
        }

        class_oid_t rel_oid = res.value;
        for (auto col : schema)
        {
            auto res_col = CreateColumnEntry(txn, rel_oid, col);
            if (!res_col.success)
            {
                return res_col;
            }
        }

        return ResultObj<class_oid_t>(oid);
    }

    bool DatabaseCatalog::DeleteTableEntry(transaction::TransactionContext *txn, class_oid_t oid)
    {
        auto chunk = classes_data_chunk_layout_->CreateDataChunk();

        access::DataChunkBuilder::BuildClassChunk(chunk, oid);

        shared::VectorValues<TupleId> tids;
        auto result = classes_index_reloid_->Get(chunk, tids);
        if (!result.success)
        {
            return false;
        }

        ResultObj<TupleId> res = txn->GetTidForModify(tids.vec, classes_->GetTableOid());
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

        if (!classes_->DeleteUndoRaw(txn, tup_id))
        {
            return false;
        }

        return true;
    }

    ResultObj<void> DatabaseCatalog::ExistsTable(transaction::TransactionContext *txn, class_oid_t oid)
    {
        auto *chunk = classes_data_chunk_layout_->CreateDataChunk();

        access::DataChunkBuilder::BuildClassChunk(chunk, oid);

        shared::VectorValues<TupleId> results;
        auto ns_res = classes_index_reloid_->Get(chunk, results);
        if (!ns_res.success)
        {
            return ResultObj<void>::Fail("Namespace does not exist");
        }

        if (!txn->GetTidExists(results.vec, classes_->GetTableOid()))
        {
            return ResultObj<void>::Fail("Namespace does not exist");
        }

        return ResultObj<void>::Ok();
    }

    bool DatabaseCatalog::UpdateTableName(transaction::TransactionContext *txn, class_oid_t oid, std::span<byte> name, namespace_oid_t namespace_oid)
    {
        if (!DeleteTableEntry(txn, oid))
        {
            return false;
        }

        auto res = CreateTableEntry(txn, name, oid, namespace_oid);
        if (!res.success)
        {
            return false;
        }

        return true;
    }

    ResultObj<class_oid_t> DatabaseCatalog::CreateIndexEntry(
        transaction::TransactionContext *txn,
        const std::span<byte> name,
        class_oid_t class_oid, // index entry in class table
        class_oid_t rel_oid,   // relation that index is for
        namespace_oid_t namespace_oid,
        access::IndexSchema &schema)
    {
        access::DataChunk *chunk = indexes_data_chunk_layout_->CreateDataChunk();

        access::DataChunkBuilder::BuildIndexChunk(
            chunk, class_oid, rel_oid, schema.IsUnique(), schema.IsPrimary(), schema.IsExclusion(),
            schema.IsImmediate(), true, true, true, u8(IndexKind::BTREE));

        TupleId tup = indexes_->Insert(txn, chunk);

        auto res_relname = indexes_index_indoid_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_relname.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to indoid index");
        }

        auto res_name = indexes_index_indrelid_->Insert(chunk, tup.GetValue());
        if (!res_name.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to indrelid index");
        }

        return ResultObj<class_oid_t>(class_oid);
    }

    ResultObj<class_oid_t> DatabaseCatalog::CreateIndex(
        transaction::TransactionContext *txn,
        const std::span<byte> name,
        class_oid_t rel_oid,
        namespace_oid_t namespace_oid,
        access::IndexSchema &schema)
    {
        if (!TryLock(txn))
            return INVALID_OID;

        auto cl_res = ExistsTable(txn, rel_oid);
        if (!cl_res.success)
        {
            return ResultObj<class_oid_t>::Fail(cl_res.message);
        }

        /* This checks if namespace still exists */
        auto table_res = CreateTable(txn, name, namespace_oid, schema);
        if (!table_res.success)
        {
            return table_res;
        }

        auto ind_res = CreateIndexEntry(txn, name, table_res.value, rel_oid, namespace_oid, schema);
        if (!ind_res.success)
        {
            return ind_res;
        }

        return ResultObj<class_oid_t>(table_res.value);
    }

    ResultObj<class_oid_t> DatabaseCatalog::CreateConstraintEntry(
        transaction::TransactionContext *txn,
        constraint_oid_t oid,
        ConstraintProps props)
    {
        access::DataChunk *chunk = constraint_data_chunk_layout_->CreateDataChunk();

        access::DataChunkBuilder::BuildConstraintChunk(chunk, oid, props);

        TupleId tup = constraints_->Insert(txn, chunk);

        auto res_oid = constraints_index_conoid_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_oid.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to conoid index");
        }

        auto res_name = constraints_index_conname_->InsertUnique(txn, chunk, tup.GetValue());
        if (!res_name.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to conname index");
        }

        auto ns_exists = ExistsNamespace(txn, props.ns_oid);
        if (!ns_exists.success)
        {
            return ResultObj<class_oid_t>::Fail(ns_exists.message);
        }

        auto res_ns = constraints_index_connamespace_->Insert(chunk, tup.GetValue());
        if (!res_name.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to connamespace index");
        }

        auto rel_exists = ExistsTable(txn, props.rel_oid);
        if (!rel_exists.success)
        {
            return ResultObj<class_oid_t>::Fail(rel_exists.message);
        }

        auto res_rel = constraints_index_conrelid_->Insert(chunk, tup.GetValue());
        if (!res_name.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to conrelid index");
        }

        auto idx_exists = ExistsTable(txn, props.ind_oid);
        if (!idx_exists.success)
        {
            return ResultObj<class_oid_t>::Fail(idx_exists.message);
        }

        auto res_idx = constraints_index_conindid_->Insert(chunk, tup.GetValue());
        if (!res_name.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to conindid index");
        }

        auto for_exists = ExistsTable(txn, props.for_oid);
        if (!for_exists.success)
        {
            return ResultObj<class_oid_t>::Fail(for_exists.message);
        }

        auto res_for = constraints_index_confrelid_->Insert(chunk, tup.GetValue());
        if (!res_for.success)
        {
            return ResultObj<class_oid_t>::Fail("Failed to insert to res_for index");
        }

        return ResultObj<class_oid_t>(oid);
    }

    ResultObj<class_oid_t> DatabaseCatalog::CreateConstraint(
        transaction::TransactionContext *txn,
        ConstraintProps props)
    {
        if (!TryLock(txn))
            return INVALID_OID;

        constraint_oid_t oid = next_constraint_oid_++;

        return CreateConstraintEntry(txn, oid, props);
    }

    void Display(
        transaction::TransactionContext *txn,
        access::DataChunkLayout *data_chunk_layout,
        access::Table *table,
        int n = 4)
    {
        u32 pid = 1;
        auto chunk = data_chunk_layout->CreateDataChunk();
        std::cout << "Select: " << std::endl;
        for (int i = 0; i < n; i++)
        {
            table->Select(txn, i, pid, chunk);
        }
        std::cout << std::endl;
    }

    void DatabaseCatalog::Select(transaction::TransactionContext *txn, int type)
    {
        switch (type)
        {
        case 0:
            Display(txn, namespace_data_chunk_layout_, namespaces_);
            break;
        case 1:
            Display(txn, classes_data_chunk_layout_, classes_);
            break;
        case 2:
            Display(txn, attribute_data_chunk_layout_, attributes_, 20);
            break;
        case 3:
            Display(txn, indexes_data_chunk_layout_, indexes_);
            break;
        case 4:
            Display(txn, constraint_data_chunk_layout_, constraints_);
            break;
        default:
            break;
        }
    }

} // namespace db7::catalog
