
#include "catalog/builder.hpp"
#include "catalog/constraints/constraint.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"
#include "catalog/database_catalog.hpp"
#include "catalog/dependency/dependency_list.hpp"
#include "catalog/entries/schema_catalog_entry.hpp"
#include "catalog/objects/column_list.hpp"
#include "common.hpp"
#include "shared/arena/fixed_bump_arena.hpp"
#include "shared/arena/object_pool.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"
#include "transaction/transaction_manager.hpp"

#include "common.hpp"
#include "parser/postgres_parser.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fmt/core.h>
#include <string>
#include <thread>
#include <vector>

using namespace db7;

static inline u64 now_ns() {
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

int main() {
  using namespace db7::catalog;

  db7::storage::DiskManagerAsync disk_mng_async(".data");

  db7::storage::DiskScheduler disk_scheduler(&disk_mng_async);
  disk_scheduler.Start();

  db7::storage::PageVersionManager version_manager;

  db7::storage::BufferPool buffer_pool(&disk_scheduler, &version_manager);

  db7::transaction::TimestampManager timestamp_manager;

  db7::shared::ObjectPool<shared::FixedBumpArena> pool(10'000, 2000);

  db7::transaction::TransactionManager txn_manager(&timestamp_manager, &buffer_pool,
                                                   &version_manager, &pool);

  auto context = txn_manager.BeginTransaction();

  auto catalogName = Identifier("katalog");
  auto catalog = DatabaseCatalog(catalogName);

  auto name = Identifier("jovan");
  auto schema = catalog.CreateSchema(*context, name);
  auto &schema_entry = schema->Cast<SchemaCatalogEntry>();

  /////

  ColumnList columns_cust;
  columns_cust.AddColumn({"id", type_id::BIGINT});
  columns_cust.AddColumn({"name", type_id::VARCHAR});

  auto tbl_name = Identifier("customers");
  CreateTableInfo customer_info(schema_entry, tbl_name);
  customer_info.columns = std::move(columns_cust);
  auto table_customer = catalog.CreateTable(*context, customer_info, schema_entry);
  auto &ss = table_customer->Cast<TableCatalogEntry>();

  CreateIndexInfo customer_index_info;
  customer_index_info.table = tbl_name;
  customer_index_info.index_name = "customer index";
  customer_index_info.constraint_type = IndexConstraintType::PRIMARY;
  customer_index_info.column_ids = {0};

  auto index_customer = catalog.CreateIndex(*context, customer_index_info, ss);

  (void)index_customer;
  ////

  ColumnList columns;
  columns.AddColumn({"order_id", type_id::BIGINT});
  columns.AddColumn({"product_name", type_id::VARCHAR});
  columns.AddColumn({"customer_id", type_id::BIGINT});

  ForeignKeyInfo key_info;
  key_info.type = ForeignKeyType::FK_TYPE_FOREIGN_KEY_TABLE;
  key_info.schema = "jovan";
  key_info.table = "customers";
  auto costraint = std::make_unique<ForeignKeyConstraint>(
      std::vector<Identifier>{
          std::vector<Identifier>{"id"},
      },
      std::vector<Identifier>{
          std::vector<Identifier>{"customer_id"},
      },
      std::move(key_info));
  std::vector<std::unique_ptr<Constraint>> constraints;
  constraints.push_back(std::move(costraint));

  LogicalDependencyList deps;

  CreateTableInfo info(schema_entry, Identifier("orders"));
  info.columns = std::move(columns);
  info.constraints = std::move(constraints);
  info.dependencies = std::move(deps);

  auto table = catalog.CreateTable(*context, info, schema_entry);
  (void)table;
  std::cout << "OK" << std::endl;
}