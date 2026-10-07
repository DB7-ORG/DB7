#pragma once

#include "catalog/entries/standard_entry.hpp"
#include "catalog/objects/create_index_info.hpp"

namespace db7::catalog {
class IndexCatalogEntryBase : public StandardEntry {
public:
  static constexpr const CatalogType Type = CatalogType::INDEX_ENTRY;
  static constexpr const char *Name = "index";

public:
  // //! The SQL of the CREATE INDEX statement
  // std::string sql;
  //! Additional index options
  // case_insensitive_map_t<Value> options;

  //! The index constraint type
  IndexConstraintType index_constraint_type;
  //! The column ids of the indexed table
  std::vector<idx_t> column_ids;

  //! Create an IndexCatalogEntry
  IndexCatalogEntryBase(DatabaseCatalog &catalog, SchemaCatalogEntryBase &schema,
                        CreateIndexInfo &info);
};
} // namespace db7::catalog