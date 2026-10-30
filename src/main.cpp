
#include "access/data_table.hpp"
#include "catalog/builder.hpp"
#include "catalog/constraints/constraint.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"
#include "catalog/constraints/unique_constraint.hpp"
#include "catalog/database_catalog.hpp"
#include "catalog/dependency/dependency_list.hpp"
#include "catalog/entries/schema_catalog_entry.hpp"
#include "catalog/entries/table_catalog_entry.hpp"
#include "catalog/objects/column_list.hpp"
#include "common.hpp"
#include "shared/arena/fixed_bump_arena.hpp"
#include "shared/arena/object_pool.hpp"
#include "storage/buffer_pool/iostream.hpp"
#include "storage/storage_manager.hpp"
#include "transaction/transaction_manager.hpp"

#include "common.hpp"
#include "parser/postgres_parser.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fmt/core.h>
#include <span>
#include <string>
#include <thread>
#include <vector>

using namespace db7;

[[maybe_unused]] static inline u64 now_ns() {
  std::atomic_signal_fence(std::memory_order_seq_cst);

  timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

  std::atomic_signal_fence(std::memory_order_seq_cst);

  return u64(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
}

using namespace db7;

template <typename Fn>
inline double run_parallel(unsigned nthreads, Fn &&fn) {
  std::atomic<unsigned> ready{0};
  std::atomic<bool> go{false};
  std::vector<std::thread> ts;
  ts.reserve(nthreads);

  for (unsigned t = 0; t < nthreads; t++) {
    ts.emplace_back([&, t] {
      ready.fetch_add(1, std::memory_order_acq_rel);
      while (!go.load(std::memory_order_acquire)) { /* spin */
      }
      fn(t);
    });
  }
  while (ready.load(std::memory_order_acquire) < nthreads) { /* spin */
  }
  auto t0 = std::chrono::steady_clock::now();
  go.store(true, std::memory_order_release);
  for (auto &th : ts) th.join();
  auto t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

inline unsigned default_threads() {
  unsigned n = std::thread::hardware_concurrency();
  return n ? n : 4;
}

int main3() {
  const std::string query = "SELECT id, name FROM users WHERE id > 10 ORDER BY name LIMIT 5";

  try {
    auto result = parser::PostgresParser::BuildParseTree(query);
    for (auto statement : result->GetStatements()) {
      // Quick way to see the whole tree
      // std::cout << statement->ToJson().dump(2) << "\n";

      if (statement->GetType() == parser::StatementType::SELECT) {
        auto select = statement.CastManagedPointerTo<parser::SelectStatement>();
        auto j = select->ToJson();
        std::cout << j.dump(2) << "\n";
        std::cout << "table: " << select->GetSelectTable()->GetTableName() << "\n";

        for (auto column : select->GetSelectColumns()) {
          column->DeriveExpressionName();
          std::cout << "column: " << column->GetExpressionName() << "\n";
        }
      }
    }
  } catch (const ParserException &e) {
    // Syntax errors and unsupported features end up here
    std::cerr << "parse error at position " << e.GetCursorPos() << ": " << e.what() << "\n";
    return 1;
  } catch (const Exception &e) {
    std::cerr << e << "\n";
    return 1;
  }
}

int main2() {
  using namespace db7::catalog;

  storage::StorageManager storage(".data");

  db7::transaction::TimestampManager timestamp_manager;

  db7::shared::ObjectPool<shared::FixedBumpArena> pool(10'000, 2000);

  db7::transaction::TransactionManager txn_manager(&timestamp_manager, &storage.GetBufferPool(),
                                                   &storage.GetVersionManager(), &pool);

  auto context = txn_manager.BeginTransaction();

  auto catalogName = Identifier("katalog");
  auto catalog = DatabaseCatalog(storage, catalogName);

  auto schema_name = Identifier("schema_default");
  auto schema = catalog.CreateSchema(*context, schema_name);
  auto &schema_entry = schema->Cast<SchemaCatalogEntry>();

  /////
  auto cust_tbl_name = Identifier("customers");
  ColumnList columns_cust;
  columns_cust.AddColumn({"id", type_id::BIGINT});
  UniqueConstraint pk({"id"}, true);
  columns_cust.AddColumn({"name", type_id::VARCHAR});

  CreateTableInfo customer_info(schema_entry, cust_tbl_name);
  customer_info.columns = std::move(columns_cust);
  customer_info.constraints.push_back(pk.Copy());
  auto table_customer = catalog.CreateTable(*context, customer_info, schema_entry);
  TableCatalogEntry &customers = table_customer->Cast<TableCatalogEntry>();

  // CreateIndexInfo customer_index_info;
  // customer_index_info.table = tbl_name;
  // customer_index_info.index_name = "customer index";
  // customer_index_info.constraint_type = IndexConstraintType::PRIMARY;
  // customer_index_info.column_ids = {0};

  // auto index_customer = catalog.CreateIndex(*context, customer_index_info, customers);

  // (void)index_customer;
  ////

  auto orders_tbl_name = Identifier("orders");
  ColumnList columns;
  columns.AddColumn({"order_id", type_id::BIGINT});
  UniqueConstraint pk2({"order_id"}, true);
  columns.AddColumn({"product_name", type_id::VARCHAR});
  columns.AddColumn({"customer_id", type_id::BIGINT});

  ForeignKeyInfo key_info;
  key_info.type = ForeignKeyType::FK_TYPE_FOREIGN_KEY_TABLE;
  key_info.schema = schema_name;
  key_info.table = cust_tbl_name;
  key_info.pk_keys = {0};
  key_info.fk_keys = {2};
  auto costraint = std::make_unique<ForeignKeyConstraint>(
      std::vector<Identifier>{
          std::vector<Identifier>{"id"},
      },
      std::vector<Identifier>{
          std::vector<Identifier>{"customer_id"},
      },
      std::move(key_info));
  std::vector<std::unique_ptr<Constraint>> constraints;
  constraints.push_back(pk2.Copy());
  constraints.push_back(std::move(costraint));

  LogicalDependencyList deps;

  CreateTableInfo info(schema_entry, orders_tbl_name);
  info.columns = std::move(columns);
  info.constraints = std::move(constraints);
  info.dependencies = std::move(deps);

  auto table_orders = catalog.CreateTable(*context, info, schema_entry);
  auto &orders = table_orders->Cast<TableCatalogEntry>();
  std::cout << "CREATED OK" << std::endl;
  {
    // insert to customer
    auto *initializer = customers.storage->Initializer();
    auto *chunk = DataChunk::BuildDataChunk(initializer, 0);
    u64 id = 1;
    std::string name = "jovan katanic";
    chunk->WriteByIdxNullCheck(0, std::span<byte>(reinterpret_cast<byte *>(&id), sizeof(u64)));
    chunk->WriteVarlenByIdx(1, std::span<byte>(reinterpret_cast<byte *>(name.data()), name.size()));
    bool r = customers.storage->Insert(*context, customers, chunk);
    if (!r) { throw CATALOG_EXCEPTION(""); }
  }
  {
    // insert to orders
    auto *initializer2 = orders.storage->Initializer();
    u64 order_id = 1;
    std::string product_name = "neki proizvod "; // sa jako dugackim imenom";
    u64 customer_id = 1;
    auto *chunk2 = DataChunk::BuildDataChunk(initializer2, 0);

    chunk2->WriteByIdxNullCheck(0,
                                std::span<byte>(reinterpret_cast<byte *>(&order_id), sizeof(u64)));
    chunk2->WriteVarlenByIdx(
        1, std::span<byte>(reinterpret_cast<byte *>(product_name.data()), product_name.size()));
    chunk2->WriteByIdxNullCheck(
        2, std::span<byte>(reinterpret_cast<byte *>(&customer_id), sizeof(u64)));
    bool r = orders.storage->Insert(*context, orders, chunk2);
    if (!r) { throw CATALOG_EXCEPTION(""); }
  }

  {
    // delete from customer
    auto entry =
        catalog.GetEntry(*context, schema_name, {CatalogType::TABLE_ENTRY, orders_tbl_name});
    TableCatalogEntry &o = entry->Cast<TableCatalogEntry>();
    bool r = o.storage->Delete(*context, o, {0, 1});
    if (!r) { throw CATALOG_EXCEPTION(""); }
  }

  {
    // delete from customer
    auto entry = catalog.GetEntry(*context, schema_name, {CatalogType::TABLE_ENTRY, cust_tbl_name});
    TableCatalogEntry &c = entry->Cast<TableCatalogEntry>();
    bool r = c.storage->Delete(*context, c, {0, 1});
    if (!r) { throw CATALOG_EXCEPTION(""); }
  }
  //////////////////////2
  {
    // insert to customer
    auto *initializer = customers.storage->Initializer();
    auto *chunk = DataChunk::BuildDataChunk(initializer, 0);
    u64 id = 1;
    std::string name = "jovan katanic";
    chunk->WriteByIdxNullCheck(0, std::span<byte>(reinterpret_cast<byte *>(&id), sizeof(u64)));
    chunk->WriteVarlenByIdx(1, std::span<byte>(reinterpret_cast<byte *>(name.data()), name.size()));
    bool r = customers.storage->Insert(*context, customers, chunk);
    if (!r) { throw CATALOG_EXCEPTION(""); }
  }

  {
    // insert to orders
    auto *initializer2 = orders.storage->Initializer();
    u64 order_id = 1;
    std::string product_name = "neki proizvod "; // sa jako dugackim imenom";
    u64 customer_id = 1;
    auto *chunk2 = DataChunk::BuildDataChunk(initializer2, 0);

    chunk2->WriteByIdxNullCheck(0,
                                std::span<byte>(reinterpret_cast<byte *>(&order_id), sizeof(u64)));
    chunk2->WriteVarlenByIdx(
        1, std::span<byte>(reinterpret_cast<byte *>(product_name.data()), product_name.size()));
    chunk2->WriteByIdxNullCheck(
        2, std::span<byte>(reinterpret_cast<byte *>(&customer_id), sizeof(u64)));
    bool r = orders.storage->Insert(*context, orders, chunk2);
    if (!r) { throw CATALOG_EXCEPTION(""); }
  }

  {
    std::vector<ChunkColumn> columns = {{1, SizeOf(db7::type_id::VARCHAR)}};
    auto initializer2 = orders.storage->Initializer(columns);
    auto *chunk = DataChunk::BuildDataChunk(initializer2.get(), 0);
    std::string product_name = "neki drugi";
    chunk->WriteVarlenByIdx(
        0, std::span<byte>(reinterpret_cast<byte *>(product_name.data()), product_name.size()));
    bool r = orders.storage->Update(*context, orders, {1, 1}, chunk);
    if (!r) { throw CATALOG_EXCEPTION(""); }
  }

  {
    std::vector<ChunkColumn> columns = {{0, SizeOf(db7::type_id::BIGINT)},
                                        {2, SizeOf(db7::type_id::BIGINT)}};
    auto initializer2 = orders.storage->Initializer(columns);
    auto *chunk = DataChunk::BuildDataChunk(initializer2.get(), 0);
    u64 order_id = 2;
    u64 customer_id = 2;
    chunk->WriteByIdxNullCheck(0,
                               std::span<byte>(reinterpret_cast<byte *>(&order_id), sizeof(u64)));
    chunk->WriteByIdxNullCheck(
        1, std::span<byte>(reinterpret_cast<byte *>(&customer_id), sizeof(u64)));
    bool r = orders.storage->Update(*context, orders, {1, 1}, chunk);
    if (!r) { throw CATALOG_EXCEPTION(""); }
  }
}

void FillTable(const char *path, u32 first, u32 npages) {
  int fd = open(path, O_WRONLY);
  DB7_ASSERT(fd >= 0, "open failed");

  constexpr u32 kChunk = 128;
  std::vector<u8> buf(size_t(kChunk) * DB7_PAGE_SIZE, 0xAB);

  const u32 end = first + npages;
  for (u32 p = first; p < end; p += kChunk) {
    u32 cnt = std::min(kChunk, end - p);
    for (u32 k = 0; k < cnt; k++) {
      u32 pid = p + k;
      std::memcpy(buf.data() + size_t(k) * DB7_PAGE_SIZE, &pid, sizeof pid);
    }
    ssize_t w = pwrite(fd, buf.data(), size_t(cnt) * DB7_PAGE_SIZE, off_t(p - 1) * DB7_PAGE_SIZE);
    DB7_ASSERT(w == ssize_t(cnt) * DB7_PAGE_SIZE, "pwrite failed");
  }

  fsync(fd);
  close(fd);
}
#include <sys/resource.h>
double CpuSeconds() {
  rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  auto s = [](timeval t) { return double(t.tv_sec) + double(t.tv_usec) / 1e6; };
  return s(ru.ru_utime) + s(ru.ru_stime);
}

int main() {
  using namespace db7::catalog;

  storage::StorageManager storage(".data");

  db7::transaction::TimestampManager timestamp_manager;

  db7::shared::ObjectPool<shared::FixedBumpArena> pool(10'000, 2000);

  db7::transaction::TransactionManager txn_manager(&timestamp_manager, &storage.GetBufferPool(),
                                                   &storage.GetVersionManager(), &pool);

  auto context = txn_manager.BeginTransaction();

  auto catalogName = Identifier("katalog");
  auto catalog = DatabaseCatalog(storage, catalogName);

  auto schema_name = Identifier("schema_default");
  auto schema = catalog.CreateSchema(*context, schema_name);
  auto &schema_entry = schema->Cast<SchemaCatalogEntry>();

  /////
  auto cust_tbl_name = Identifier("customers");
  ColumnList columns_cust;
  columns_cust.AddColumn({"id", type_id::BIGINT});
  UniqueConstraint pk({"id"}, true);
  columns_cust.AddColumn({"name", type_id::VARCHAR});

  CreateTableInfo customer_info(schema_entry, cust_tbl_name);
  customer_info.columns = std::move(columns_cust);
  customer_info.constraints.push_back(pk.Copy());
  auto table_customer = catalog.CreateTable(*context, customer_info, schema_entry);
  TableCatalogEntry &customers = table_customer->Cast<TableCatalogEntry>();

  std::cout << customers.storage->GetTableId() << std::endl;

  constexpr u32 kThreads = 16;      // ≈ batches in flight; try 1, 4, 16, 32
  constexpr u32 kPages = 64'000;    // 500 MB; each page is read exactly once
  constexpr u32 kN = 8;             // pages per batch
  constexpr bool kVectored = false; // false = solution A, true = solution B
  constexpr u32 kFirstPid = 1 + 64; // skip pages the catalog may own

  const u32 tbl = u32(customers.storage->GetTableId());
  char path[256];
  std::snprintf(path, sizeof path, ".data/table_%u.db", tbl);
  FillTable(path, kFirstPid, kPages);

  auto &buffer_pool = storage.GetBufferPool();
  const u32 nbatches = kPages / kN;
  std::atomic<bool> go{false};

  auto worker = [&](u32 t) {
    storage::Page *state[IO_MAX_COMBINE];
    while (!go.load(std::memory_order_acquire)) std::this_thread::yield();

    // this thread's batches: [b_begin, b_end)
    const u32 per = nbatches / kThreads;
    const u32 b_begin = t * per;
    const u32 b_end = (t == kThreads - 1) ? nbatches : b_begin + per;

    u32 next_pg = kFirstPid + b_begin * kN;
    const u32 pg_end = kFirstPid + b_end * kN;
    storage::AccessStrategy strategy(db7::storage::AccessStrategyType::NORMAL);
    storage::ReadIoStream stream(
        &buffer_pool, strategy, tbl,
        [&next_pg, pg_end]() { return next_pg < pg_end ? next_pg++ : storage::INVALID_BLOCK; }, 32);

    for (u32 b = b_begin; b < b_end; b++) {
      u32 first = kFirstPid + b * kN;

      if (kVectored) {
        for (u32 done = 0; done < kN;) { // PinRange may stop early at a hit
          auto r =
              buffer_pool.PinRange({tbl, first + done}, kN - done, state + done, strategy, true);
          done += r.count;
        }
      } else {
        for (u32 j = 0; j < kN; j++) {
          state[j] = buffer_pool.Pin({tbl, first + j}, strategy, true);
        }
      }

      for (u32 j = 0; j < kN; j++) {
        state[j]->WaitIO();
        u32 stamp;
        std::memcpy(&stamp, state[j]->GetData(), sizeof stamp);
        if (stamp != first + j) {
          std::fprintf(stderr, "wrong page: expected %u got %u\n", first + j, stamp);
          std::abort();
        }
        buffer_pool.Unpin(state[j], false);
      }
    }
  };

  std::vector<std::thread> threads;
  for (u32 t = 0; t < kThreads; t++) threads.emplace_back(worker, t);

  double cpu0 = CpuSeconds();
  auto start = now_ns();
  go.store(true, std::memory_order_release); // all threads start together
  for (auto &t : threads) t.join();
  auto end = now_ns();
  double cpu = CpuSeconds() - cpu0;

  double secs = (end - start) / 1e9;
  double mb = double(kPages) * DB7_PAGE_SIZE / 1e6;
  printf("%s threads=%u n=%u: %.1f ms, %.0f MB/s, cpu %.2f us/page\n",
         kVectored ? "readv" : "single", kThreads, kN, secs * 1e3, mb / secs, cpu * 1e6 / kPages);

  storage.GetDiskScheduler().Stop();
}
