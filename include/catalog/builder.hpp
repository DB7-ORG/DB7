#pragma once

#include "access/schema.hpp"
#include "catalog_common.hpp"

namespace db7::storage {
class BufferPool; // forward declare, no #include needed
class DiskManagerAsync;
} // namespace db7::storage

namespace db7::catalog {
/// forward refs
class DatabaseCatalog;

class Builder {
public:
  static DatabaseCatalog *
  CreateDatabaseCatalog(storage::BufferPool *buffer_pool,
                        storage::DiskManagerAsync *disk_mng, db_oid_t oid);

  static access::Schema CreateDatabaseSchema();
  static access::Schema CreateNamespaceSchema();
  static access::Schema CreateClassSchema();
  static access::Schema CreateAttributeSchema();
  static access::Schema CreateIndexSchema();
  static access::Schema CreateTypeSchema();
  static access::Schema CreateConstraintSchema();
  static access::Schema CreateLanguageSchema();
  static access::Schema CreateProcSchema();
};
} // namespace db7::catalog