#include <gtest/gtest.h>

#include "access/data_table.hpp"
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
#include <string>
#include <vector>

using namespace db7;
using namespace db7::catalog;

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

  void Insert(Txn &txn, int64_t id) {
    const std::string name = Name(id);
    DataChunkPtr row(DataChunk::BuildDataChunk(Table().Initializer(), name.size()));
    row->WriteByIdx(0, {reinterpret_cast<const byte *>(&id), sizeof(id)});
    row->WriteVarlenByIdx(1, {reinterpret_cast<const byte *>(name.data()), name.size()});
    ASSERT_TRUE(Table().Insert(txn, *customers_, row.get())) << "insert id " << id;
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
      if (res == access::ScanResult::Blocked) {
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
      if (res == access::ScanResult::Finished) break;
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

  access::ScanResult res;
  do { res = Table().Scan(*local); } while (res != access::ScanResult::Finished);

  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(Table().Scan(*local), access::ScanResult::Finished);
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
  access::ScanResult res;
  do {
    res = Table().Scan(*local);
    if (res == access::ScanResult::Blocked) {
      std::this_thread::yield();
      continue;
    }
    ColumnsChunk &chunk = local->Chunk();
    ASSERT_EQ(chunk.GetColCount(), 1);
    for (u16 i = 0; i < chunk.SelCount(); ++i)
      names.push_back(ReadStr(chunk, 0, chunk.SelData()[i]));
  } while (res != access::ScanResult::Finished);

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

} // namespace