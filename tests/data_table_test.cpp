#include <gtest/gtest.h>

#include "access/data_table.hpp"
#include "access/index/btree.hpp"
#include "access/objects/add_table_index_info.hpp"
#include "catalog/database_catalog.hpp"
#include "catalog/entries/table_catalog_entry.hpp"
#include "storage/columns_chunk.hpp"
#include "storage/data_chunk.hpp"
#include "storage/storage_manager.hpp"
#include "transaction/transaction_manager.hpp"
// Adjust these to wherever they live in your tree:
#include "catalog/constraints/unique_constraint.hpp"
#include "catalog/entries/schema_catalog_entry.hpp"
#include "catalog/objects/create_table_info.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace db7;
using namespace db7::catalog;
using db7::ScanResult;

namespace {

// =========================================================================
// 1. HELPERS
// =========================================================================

constexpr const char *kDataDir = ".data_scan_test";

constexpr column_t kIdOid = 0;
constexpr column_t kNameOid = 1;

/// Odd ids get short (inlined) names, even ids get long (heap) names,
/// so every chunk mixes both varlen forms.
std::string Name(int64_t id) {
  return id % 2 ? "c" + std::to_string(id) : "cu " + std::to_string(id);
}

/// Value written by updates: short, so it always stays inline, and never
/// equal to Name(id).
std::string NewName(int64_t id) { return "u" + std::to_string(id); }

struct Row {
  int64_t id;
  std::string name;
  bool operator<(const Row &o) const { return id < o.id; }
};

int64_t ReadI64(ColumnsChunk &chunk, u16 idx, u16 row) {
  int64_t v;
  std::memcpy(&v, chunk.GetColumnByIdx(idx) + row * sizeof(int64_t), sizeof(v));
  return v;
}

/// Varlen slot: [u16 len][inline bytes] or [u16 len][6-byte prefix][u64 pointer].
std::string ReadStr(ColumnsChunk &chunk, u16 idx, u16 row) {
  const byte *slot = chunk.GetColumnByIdx(idx) + row * storage::VarlenEntry::slot_size;
  u16 len;
  std::memcpy(&len, slot, sizeof(len));
  if (len <= storage::VarlenEntry::inline_bytes) {
    return {reinterpret_cast<const char *>(slot + sizeof(u16)), len};
  }
  u64 ref;
  std::memcpy(&ref, slot + sizeof(u16) + 6, sizeof(ref));
  return {reinterpret_cast<const char *>(static_cast<uintptr_t>(ref)), len};
}

// =========================================================================
// 2. FIXTURE
// =========================================================================

class DataTableScanTest : public ::testing::Test {
protected:
  using Txn = transaction::TransactionContext;

  std::unique_ptr<storage::StorageManager> storage_;
  std::unique_ptr<transaction::TimestampManager> timestamps_;
  std::unique_ptr<shared::ObjectPool<shared::FixedBumpArena>> pool_;
  std::unique_ptr<transaction::TransactionManager> txns_;
  std::unique_ptr<DatabaseCatalog> catalog_;
  TableCatalogEntry *customers_ = nullptr;

  void SetUp() override {
    std::filesystem::remove_all(kDataDir); // a leftover file would make the table non-empty

    storage_ = std::make_unique<storage::StorageManager>(kDataDir);
    timestamps_ = std::make_unique<transaction::TimestampManager>();
    pool_ = std::make_unique<shared::ObjectPool<shared::FixedBumpArena>>(10'000, 2000);
    txns_ = std::make_unique<transaction::TransactionManager>(
        timestamps_.get(), &storage_->GetBufferPool(), &storage_->GetVersionManager(), pool_.get());

    Txn *setup = txns_->BeginTransaction();
    catalog_ = std::make_unique<DatabaseCatalog>(*storage_, Identifier("katalog"));

    auto schema = catalog_->CreateSchema(*setup, Identifier("schema_default"));
    auto &schema_entry = schema->Cast<SchemaCatalogEntry>();

    ColumnList cols;
    cols.AddColumn({"id", type_id::BIGINT});
    cols.AddColumn({"name", type_id::VARCHAR});
    UniqueConstraint pk({"id"}, true);

    CreateTableInfo info(schema_entry, Identifier("customers"));
    info.columns = std::move(cols);
    info.constraints.push_back(pk.Copy());
    auto table = catalog_->CreateTable(*setup, info, schema_entry);
    customers_ = &table->Cast<TableCatalogEntry>();

    txns_->Commit(setup);
  }

  void TearDown() override {
    catalog_.reset();
    txns_.reset();
    pool_.reset();
    timestamps_.reset();
    storage_.reset();
    std::filesystem::remove_all(kDataDir);
    db7::shared::detail::ResetQueueId();
  }

  access::DataTable &Table() { return *customers_->storage; }

  Txn *Begin() { return txns_->BeginTransaction(); }
  void Commit(Txn *txn) { txns_->Commit(txn); }

  DataChunkPtr MakeRow(int64_t id, const std::string &name) {
    DataChunkPtr row(DataChunk::BuildDataChunk(Table().Initializer(), name.size()));
    row->WriteByIdx(0, {reinterpret_cast<const byte *>(&id), sizeof(id)});
    row->WriteVarlenByIdx(1, {reinterpret_cast<const byte *>(name.data()), name.size()});
    return row;
  }

  void Insert(Txn &txn, int64_t id) {
    DataChunkPtr row = MakeRow(id, Name(id));
    ASSERT_TRUE(Table().Insert(txn, *customers_, row.get())) << "insert id " << id;
  }

  /// Finds where `id` lives through the primary key index, the same way
  /// DataTable's FK check looks rows up. Delete doesn't remove index entries,
  /// and no test inserts a key twice or moves a key back to an old value, so
  /// each key has exactly one entry.
  TupleId Tid(int64_t id) {
    DataChunkPtr key = MakeRow(id, Name(id)); // only the id column is read
    for (auto &index : Table().Indexes()) {
      if (index->type_ != IndexConstraintType::PRIMARY) continue;

      auto &btree = index->index_->Cast<access::BTreeIndex<>>();
      shared::VectorValues<TupleId> results;
      auto res = btree.Get(key.get(), results);
      EXPECT_TRUE(res.success) << res.message;
      EXPECT_EQ(results.vec.size(), 1u) << "index entries for id " << id;

      const TupleId tid = results.vec.empty() ? INVALID_TID : results.vec[0];
      EXPECT_NE(tid.GetPageId(), 0u) << "id " << id << " maps to page 0";
      return tid;
    }
    ADD_FAILURE() << "customers has no primary key index";
    return INVALID_TID;
  }

  // -----------------------------------------------------------------------
  // Delete / Update wrappers. Both return false on a write-write conflict
  // (row locked by another txn).
  // -----------------------------------------------------------------------
  bool Delete(Txn &txn, int64_t id) { return Table().Delete(txn, *customers_, Tid(id)); }

  /// Updates only the name column: no indexed column changes, so DataTable
  /// updates in place and the row keeps its TupleId.
  bool Update(Txn &txn, int64_t id, const std::string &name) {
    std::vector<storage::StorageColumnDesc> cols;
    cols.emplace_back(kNameOid, type_id::VARCHAR, storage::DefaultValue{}, false, true);
    DataChunkPtr init = Table().Initializer(cols);

    DataChunkPtr row(DataChunk::BuildDataChunk(init.get(), name.size()));
    row->WriteVarlenByIdx(0, {reinterpret_cast<const byte *>(name.data()), name.size()});

    return Table().Update(txn, *customers_, Tid(id), row.get());
  }

  /// Changes the primary key of `id` to `new_id`, and the name too if given.
  /// The key is indexed, so DataTable takes the delete + insert path and the
  /// row gets a new TupleId; Tid(new_id) finds it through the index.
  bool UpdateKey(Txn &txn, int64_t id, int64_t new_id,
                 const std::optional<std::string> &name = std::nullopt) {
    std::vector<storage::StorageColumnDesc> cols;
    cols.emplace_back(kIdOid, type_id::BIGINT);
    if (name) cols.emplace_back(kNameOid, type_id::VARCHAR, storage::DefaultValue{}, false, true);
    DataChunkPtr init = Table().Initializer(cols);

    DataChunkPtr row(DataChunk::BuildDataChunk(init.get(), name ? name->size() : 0));
    row->WriteByIdx(0, {reinterpret_cast<const byte *>(&new_id), sizeof(new_id)});
    if (name)
      row->WriteVarlenByIdx(1, {reinterpret_cast<const byte *>(name->data()), name->size()});

    return Table().Update(txn, *customers_, Tid(id), row.get());
  }

  void InsertCommitted(int64_t from, int64_t to) {
    Txn *txn = Begin();
    for (int64_t id = from; id < to; ++id) Insert(*txn, id);
    Commit(txn);
  }

  static std::vector<storage::StorageColumnDesc> AllColumns() {
    std::vector<storage::StorageColumnDesc> cols;
    cols.emplace_back(kIdOid, type_id::BIGINT);
    cols.emplace_back(kNameOid, type_id::VARCHAR, storage::DefaultValue{}, false, true);
    return cols;
  }

  /// Runs a full scan for `txn` and returns every selected row, sorted by id.
  /// Also checks the per-chunk invariants on the way.
  std::vector<Row> ScanAll(Txn &txn) {
    auto shared = Table().InitParallelScan(txn, AllColumns());
    auto local = Table().InitLocalScan(*shared);

    std::vector<Row> rows;
    for (size_t calls = 0; calls < 1'000'000; ++calls) {
      const auto res = Table().Scan(*local);
      if (res == ScanResult::Blocked) {
        std::this_thread::yield();
        continue;
      }

      ColumnsChunk &chunk = local->Chunk();
      EXPECT_LE(chunk.GetTupleCount(), STANDARD_VECTOR_SIZE);
      EXPECT_LE(chunk.SelCount(), chunk.GetTupleCount());

      for (u16 i = 0; i < chunk.SelCount(); ++i) {
        const u16 r = chunk.SelData()[i];
        EXPECT_LT(r, chunk.GetTupleCount());
        rows.push_back({ReadI64(chunk, 0, r), ReadStr(chunk, 1, r)});
      }
      if (res == ScanResult::Finished) break;
    }
    std::sort(rows.begin(), rows.end());
    return rows;
  }

  /// Every id in [from, to) exactly once, each with its own name.
  static void ExpectExactly(const std::vector<Row> &rows, int64_t from, int64_t to) {
    ASSERT_EQ(rows.size(), static_cast<size_t>(to - from));
    for (int64_t id = from; id < to; ++id) {
      SCOPED_TRACE(id);
      const Row &r = rows[id - from];
      EXPECT_EQ(r.id, id) << "missing or duplicated row";
      EXPECT_EQ(r.name, Name(id));
    }
  }

  /// Rows [from, to) with their original names.
  static std::vector<Row> Range(int64_t from, int64_t to) {
    std::vector<Row> out;
    for (int64_t id = from; id < to; ++id) out.push_back({id, Name(id)});
    return out;
  }

  /// `rows` (sorted, from ScanAll) must equal `expected` exactly: same ids,
  /// each once, same names. Catches a stale version returned next to a new one.
  static void ExpectRows(const std::vector<Row> &rows, std::vector<Row> expected) {
    std::sort(expected.begin(), expected.end());
    ASSERT_EQ(rows.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      SCOPED_TRACE(expected[i].id);
      EXPECT_EQ(rows[i].id, expected[i].id) << "missing or duplicated row";
      EXPECT_EQ(rows[i].name, expected[i].name);
    }
  }
};

// =========================================================================
// 3. BASIC SCANS
// =========================================================================

TEST_F(DataTableScanTest, EmptyTableFinishesWithNoRows) {
  Txn *txn = Begin();
  EXPECT_TRUE(ScanAll(*txn).empty());
}

TEST_F(DataTableScanTest, CommittedRowsAreReturnedOnceWithTheirValues) {
  InsertCommitted(0, 100);

  Txn *txn = Begin();
  ExpectExactly(ScanAll(*txn), 0, 100);
}

/// More rows than one chunk holds and more than one page holds, so the scan
/// has to continue a page across chunks and pull several pages into a chunk.
/// Depends on the FSM handing out a new page when the current one is full.
TEST_F(DataTableScanTest, ScanSpansMultipleChunksAndPages) {
  constexpr int64_t kRows = 3 * STANDARD_VECTOR_SIZE + 17;
  InsertCommitted(0, kRows);

  Txn *txn = Begin();
  ExpectExactly(ScanAll(*txn), 0, kRows);
}

TEST_F(DataTableScanTest, ScanAfterFinishedStaysFinished) {
  InsertCommitted(0, 10);

  Txn *txn = Begin();
  auto shared = Table().InitParallelScan(*txn, AllColumns());
  auto local = Table().InitLocalScan(*shared);

  ScanResult res;
  do { res = Table().Scan(*local); } while (res != ScanResult::Finished);

  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(Table().Scan(*local), ScanResult::Finished);
    EXPECT_EQ(local->Chunk().SelCount(), 0);
  }
}

/// A scan that projects one column gets a one-column chunk with the right values.
TEST_F(DataTableScanTest, ProjectionReturnsOnlyRequestedColumn) {
  InsertCommitted(0, 50);

  Txn *txn = Begin();
  std::vector<storage::StorageColumnDesc> only_name;
  only_name.emplace_back(kNameOid, type_id::VARCHAR, storage::DefaultValue{}, false, true);
  auto shared = Table().InitParallelScan(*txn, std::move(only_name));
  auto local = Table().InitLocalScan(*shared);

  std::vector<std::string> names;
  ScanResult res;
  do {
    res = Table().Scan(*local);
    if (res == ScanResult::Blocked) {
      std::this_thread::yield();
      continue;
    }
    ColumnsChunk &chunk = local->Chunk();
    ASSERT_EQ(chunk.GetColCount(), 1);
    for (u16 i = 0; i < chunk.SelCount(); ++i)
      names.push_back(ReadStr(chunk, 0, chunk.SelData()[i]));
  } while (res != ScanResult::Finished);

  std::vector<std::string> expected;
  for (int64_t id = 0; id < 50; ++id) expected.push_back(Name(id));
  std::sort(names.begin(), names.end());
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(names, expected);
}

// =========================================================================
// 4. VISIBILITY
// =========================================================================

TEST_F(DataTableScanTest, OwnUncommittedInsertsAreVisible) {
  Txn *writer = Begin();
  for (int64_t id = 0; id < 20; ++id) Insert(*writer, id);

  ExpectExactly(ScanAll(*writer), 0, 20);
}

TEST_F(DataTableScanTest, OtherTxnsUncommittedInsertsAreInvisible) {
  Txn *writer = Begin();
  for (int64_t id = 0; id < 20; ++id) Insert(*writer, id);

  Txn *reader = Begin();
  EXPECT_TRUE(ScanAll(*reader).empty()) << "dirty read";
}

TEST_F(DataTableScanTest, RowsCommittedAfterSnapshotAreInvisible) {
  InsertCommitted(0, 10);

  Txn *old_reader = Begin(); // snapshot sees ids 0..9
  InsertCommitted(10, 30);
  Txn *new_reader = Begin(); // snapshot sees ids 0..29

  ExpectExactly(ScanAll(*old_reader), 0, 10);
  ExpectExactly(ScanAll(*new_reader), 0, 30);
}

/// Committed and uncommitted rows on the same page: the reader must get
/// exactly the committed ones, which exercises the selection vector.
TEST_F(DataTableScanTest, MixedPageSelectsOnlyVisibleRows) {
  InsertCommitted(0, 10);
  Txn *writer = Begin();
  for (int64_t id = 10; id < 20; ++id) Insert(*writer, id); // same page, uncommitted

  Txn *reader = Begin();
  ExpectExactly(ScanAll(*reader), 0, 10);
}

// =========================================================================
// 5. DELETE
// =========================================================================

TEST_F(DataTableScanTest, DeletedRowsAreNotReturned) {
  InsertCommitted(0, 100);

  Txn *deleter = Begin();
  for (int64_t id = 0; id < 100; id += 2) ASSERT_TRUE(Delete(*deleter, id)) << id;
  Commit(deleter);

  std::vector<Row> expected;
  for (int64_t id = 1; id < 100; id += 2) expected.push_back({id, Name(id)});

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), expected);
}

TEST_F(DataTableScanTest, DeletingEveryRowLeavesAnEmptyScan) {
  InsertCommitted(0, 30);

  Txn *deleter = Begin();
  for (int64_t id = 0; id < 30; ++id) ASSERT_TRUE(Delete(*deleter, id)) << id;
  Commit(deleter);

  Txn *reader = Begin();
  EXPECT_TRUE(ScanAll(*reader).empty());
}

/// Deletes spread over several pages and chunks, including the last page.
TEST_F(DataTableScanTest, DeletesAcrossPagesAndChunks) {
  constexpr int64_t kRows = 3 * STANDARD_VECTOR_SIZE + 17;
  InsertCommitted(0, kRows);

  Txn *deleter = Begin();
  for (int64_t id = 0; id < kRows; id += 3) ASSERT_TRUE(Delete(*deleter, id)) << id;
  Commit(deleter);

  std::vector<Row> expected;
  for (int64_t id = 0; id < kRows; ++id)
    if (id % 3 != 0) expected.push_back({id, Name(id)});

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), expected);
}

TEST_F(DataTableScanTest, OwnUncommittedDeleteIsInvisibleToSelf) {
  InsertCommitted(0, 10);

  Txn *deleter = Begin();
  ASSERT_TRUE(Delete(*deleter, 3));
  ASSERT_TRUE(Delete(*deleter, 4));

  std::vector<Row> expected = Range(0, 10);
  std::erase_if(expected, [](const Row &r) { return r.id == 3 || r.id == 4; });
  ExpectRows(ScanAll(*deleter), expected);
}

TEST_F(DataTableScanTest, OtherTxnsUncommittedDeleteIsIgnored) {
  InsertCommitted(0, 10);

  Txn *deleter = Begin();
  ASSERT_TRUE(Delete(*deleter, 3));

  Txn *reader = Begin();
  ExpectExactly(ScanAll(*reader), 0, 10);
}

TEST_F(DataTableScanTest, DeleteCommittedAfterSnapshotIsIgnoredByOldReader) {
  InsertCommitted(0, 10);

  Txn *old_reader = Begin();
  Txn *deleter = Begin();
  ASSERT_TRUE(Delete(*deleter, 3));
  Commit(deleter);
  Txn *new_reader = Begin();

  ExpectExactly(ScanAll(*old_reader), 0, 10);
  ExpectRows(ScanAll(*new_reader), [] {
    auto r = Range(0, 10);
    std::erase_if(r, [](const Row &x) { return x.id == 3; });
    return r;
  }());
}

TEST_F(DataTableScanTest, DeletingARowLockedByAnotherTxnFails) {
  InsertCommitted(0, 10);

  Txn *first = Begin();
  ASSERT_TRUE(Delete(*first, 3));

  Txn *second = Begin();
  EXPECT_FALSE(Delete(*second, 3)) << "write-write conflict not detected";
}

// =========================================================================
// 6. UPDATE
// =========================================================================

/// Every row comes back once: an update must not leave the old version
/// visible next to the new one.
TEST_F(DataTableScanTest, UpdatedRowsReturnNewValueExactlyOnce) {
  InsertCommitted(0, 100);

  Txn *updater = Begin();
  for (int64_t id = 0; id < 100; id += 2) ASSERT_TRUE(Update(*updater, id, NewName(id))) << id;
  Commit(updater);

  std::vector<Row> expected = Range(0, 100);
  for (Row &r : expected)
    if (r.id % 2 == 0) r.name = NewName(r.id);

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), expected);
}

TEST_F(DataTableScanTest, UpdatesAcrossPagesAndChunks) {
  constexpr int64_t kRows = 3 * STANDARD_VECTOR_SIZE + 17;
  InsertCommitted(0, kRows);

  Txn *updater = Begin();
  for (int64_t id = 1; id < kRows; id += 2) ASSERT_TRUE(Update(*updater, id, NewName(id))) << id;
  Commit(updater);

  std::vector<Row> expected = Range(0, kRows);
  for (Row &r : expected)
    if (r.id % 2 == 1) r.name = NewName(r.id);

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), expected);
}

TEST_F(DataTableScanTest, OwnUncommittedUpdateIsVisibleToSelf) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(Update(*updater, 3, NewName(3)));

  std::vector<Row> expected = Range(0, 10);
  expected[3].name = NewName(3);
  ExpectRows(ScanAll(*updater), expected);
}

TEST_F(DataTableScanTest, OtherTxnsUncommittedUpdateShowsOldValue) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(Update(*updater, 3, NewName(3)));

  Txn *reader = Begin();
  ExpectExactly(ScanAll(*reader), 0, 10);
}

TEST_F(DataTableScanTest, UpdateCommittedAfterSnapshotShowsOldValueToOldReader) {
  InsertCommitted(0, 10);

  Txn *old_reader = Begin();
  Txn *updater = Begin();
  ASSERT_TRUE(Update(*updater, 3, NewName(3)));
  Commit(updater);
  Txn *new_reader = Begin();

  ExpectExactly(ScanAll(*old_reader), 0, 10);

  std::vector<Row> expected = Range(0, 10);
  expected[3].name = NewName(3);
  ExpectRows(ScanAll(*new_reader), expected);
}

TEST_F(DataTableScanTest, UpdatingTwiceInOneTxnKeepsTheLastValue) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(Update(*updater, 3, "first"));
  ASSERT_TRUE(Update(*updater, 3, "second"));
  Commit(updater);

  std::vector<Row> expected = Range(0, 10);
  expected[3].name = "second";

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), expected);
}

TEST_F(DataTableScanTest, UpdateThenDeleteInOneTxnRemovesTheRow) {
  InsertCommitted(0, 10);

  Txn *writer = Begin();
  ASSERT_TRUE(Update(*writer, 3, NewName(3)));
  ASSERT_TRUE(Delete(*writer, 3));
  Commit(writer);

  std::vector<Row> expected = Range(0, 10);
  std::erase_if(expected, [](const Row &r) { return r.id == 3; });

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), expected);
}

TEST_F(DataTableScanTest, UpdatingARowLockedByAnotherTxnFails) {
  InsertCommitted(0, 10);

  Txn *first = Begin();
  ASSERT_TRUE(Update(*first, 3, NewName(3)));

  Txn *second = Begin();
  EXPECT_FALSE(Update(*second, 3, "conflict")) << "write-write conflict not detected";
}

// =========================================================================
// 7. UPDATE OF AN INDEXED COLUMN (delete + insert path)
// =========================================================================

/// Rows [0, 10) with `from` renamed to key `to`, keeping or replacing its name.
std::vector<Row> RangeWithKeyMoved(int64_t from, int64_t to,
                                   std::optional<std::string> name = std::nullopt) {
  std::vector<Row> rows;
  for (int64_t id = 0; id < 10; ++id) {
    if (id == from)
      rows.push_back({to, name ? *name : Name(from)});
    else
      rows.push_back({id, Name(id)});
  }
  return rows;
}

/// The old key disappears and the row shows up under the new key with its
/// old name: the merge copied the untouched column, and the old version is
/// not returned next to the new one.
TEST_F(DataTableScanTest, KeyUpdateMovesRowToNewKey) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(UpdateKey(*updater, 3, 103));
  Commit(updater);

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), RangeWithKeyMoved(3, 103));
}

TEST_F(DataTableScanTest, KeyAndNameUpdatedTogether) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(UpdateKey(*updater, 3, 103, NewName(103)));
  Commit(updater);

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), RangeWithKeyMoved(3, 103, NewName(103)));
}

TEST_F(DataTableScanTest, OwnUncommittedKeyUpdateIsVisibleToSelf) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(UpdateKey(*updater, 3, 103));

  ExpectRows(ScanAll(*updater), RangeWithKeyMoved(3, 103));
}

TEST_F(DataTableScanTest, OtherTxnsUncommittedKeyUpdateShowsOldRow) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(UpdateKey(*updater, 3, 103));

  Txn *reader = Begin();
  ExpectExactly(ScanAll(*reader), 0, 10);
}

TEST_F(DataTableScanTest, KeyUpdateCommittedAfterSnapshotShowsOldRowToOldReader) {
  InsertCommitted(0, 10);

  Txn *old_reader = Begin();
  Txn *updater = Begin();
  ASSERT_TRUE(UpdateKey(*updater, 3, 103));
  Commit(updater);
  Txn *new_reader = Begin();

  ExpectExactly(ScanAll(*old_reader), 0, 10);
  ExpectRows(ScanAll(*new_reader), RangeWithKeyMoved(3, 103));
}

TEST_F(DataTableScanTest, KeyUpdatedTwiceInOneTxnKeepsTheLastKey) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(UpdateKey(*updater, 3, 103));
  ASSERT_TRUE(UpdateKey(*updater, 103, 203));
  Commit(updater);

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), RangeWithKeyMoved(3, 203));
}

/// After the move, the row is reachable through the new key: the index
/// points at the new TupleId and the moved row can be written again.
TEST_F(DataTableScanTest, MovedRowCanBeUpdatedThroughItsNewKey) {
  InsertCommitted(0, 10);

  Txn *mover = Begin();
  ASSERT_TRUE(UpdateKey(*mover, 3, 103));
  Commit(mover);

  Txn *updater = Begin();
  ASSERT_TRUE(Update(*updater, 103, NewName(103)));
  Commit(updater);

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), RangeWithKeyMoved(3, 103, NewName(103)));
}

TEST_F(DataTableScanTest, MovedRowCanBeDeletedThroughItsNewKey) {
  InsertCommitted(0, 10);

  Txn *mover = Begin();
  ASSERT_TRUE(UpdateKey(*mover, 3, 103));
  Commit(mover);

  Txn *deleter = Begin();
  ASSERT_TRUE(Delete(*deleter, 103));
  Commit(deleter);

  std::vector<Row> expected = Range(0, 10);
  std::erase_if(expected, [](const Row &r) { return r.id == 3; });

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), expected);
}

/// Many moves on the same page. Each move takes a new slot without freeing
/// the old one, so this also checks the page has room for the new versions.
TEST_F(DataTableScanTest, KeyUpdatesOfManyRows) {
  InsertCommitted(0, 100);

  Txn *updater = Begin();
  for (int64_t id = 0; id < 100; id += 2) ASSERT_TRUE(UpdateKey(*updater, id, id + 1000)) << id;
  Commit(updater);

  std::vector<Row> expected;
  for (int64_t id = 0; id < 100; ++id) expected.push_back({id % 2 == 0 ? id + 1000 : id, Name(id)});

  Txn *reader = Begin();
  ExpectRows(ScanAll(*reader), expected);
}

/// `UPDATE ... SET id = id`: the key doesn't change, so it must not count
/// as a duplicate of itself.
TEST_F(DataTableScanTest, KeyUpdateToTheSameValueSucceeds) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  ASSERT_TRUE(UpdateKey(*updater, 3, 3));
  Commit(updater);

  Txn *reader = Begin();
  ExpectExactly(ScanAll(*reader), 0, 10);
}

/// Moving a row onto a key another row already has violates the primary key.
/// No abort yet, so the table isn't scanned afterwards.
TEST_F(DataTableScanTest, KeyUpdateToAnExistingKeyIsRejected) {
  InsertCommitted(0, 10);

  Txn *updater = Begin();
  EXPECT_ANY_THROW(UpdateKey(*updater, 3, 5)) << "duplicate primary key accepted";
}

TEST_F(DataTableScanTest, KeyUpdateOfARowLockedByAnotherTxnFails) {
  InsertCommitted(0, 10);

  Txn *first = Begin();
  ASSERT_TRUE(Update(*first, 3, NewName(3)));

  Txn *second = Begin();
  EXPECT_FALSE(UpdateKey(*second, 3, 103)) << "write-write conflict not detected";
}

} // namespace