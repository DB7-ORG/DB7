#pragma once

#include "catalog/catalog_common.hpp"
#include "catalog/database_catalog.hpp"
#include "shared/identifier.hpp"
#include "shared/pointers/optional_ptr.hpp"
#include "transaction/transaction_common.hpp"

#include <atomic>
#include <string>

namespace db7::catalog {

class DatabaseCatalog;
class CatalogSet;
class SchemaCatalogEntry;

class CatalogEntry {
public:
  //! The oid of the entry
  idx_t oid;
  //! The type of this catalog entry
  CatalogType type;
  //! Reference to the catalog set this entry is stored in
  optional_ptr<CatalogSet> set;
  //! The name of the entry
  Identifier name;
  //! Whether or not the object is deleted
  bool deleted;
  //! Whether or not the object is temporary and should not be added to the WAL
  bool temporary;
  //! Whether or not the entry is an internal entry (cannot be deleted, not dumped, etc)
  bool internal;
  //! The name of the extension that registered this entry (empty for core entries)
  Identifier extension_name;
  //! Timestamp at which the catalog entry was created
  std::atomic<timestamp_t> timestamp;

private:
  //! Child entry
  std::unique_ptr<CatalogEntry> child;
  //! Parent entry (the node that dependents_map this node)
  std::atomic<CatalogEntry *> parent;

public:
  CatalogEntry(CatalogType type, DatabaseCatalog &catalog, Identifier name);
  CatalogEntry(CatalogType type, Identifier name, idx_t oid);
  virtual ~CatalogEntry();

  virtual DatabaseCatalog &ParentCatalog();
  virtual const DatabaseCatalog &ParentCatalog() const;
  virtual SchemaCatalogEntry &ParentSchema();
  virtual const SchemaCatalogEntry &ParentSchema() const;

public:
  void SetChild(std::unique_ptr<CatalogEntry> child);
  std::unique_ptr<CatalogEntry> TakeChild();
  bool HasChild() const;
  bool HasParent() const;
  CatalogEntry &Child();
  CatalogEntry &Parent();
  const CatalogEntry &Parent() const;

public:
  template <class TARGET>
  TARGET &Cast() {
    return reinterpret_cast<TARGET &>(*this);
  }
  template <class TARGET>
  const TARGET &Cast() const {
    return reinterpret_cast<const TARGET &>(*this);
  }
};

class InCatalogEntry : public CatalogEntry {
public:
  InCatalogEntry(CatalogType type, DatabaseCatalog &catalog, Identifier name);
  ~InCatalogEntry() override;

  //! The catalog the entry belongs to
  DatabaseCatalog &catalog;

public:
  DatabaseCatalog &ParentCatalog() override { return catalog; }
  const DatabaseCatalog &ParentCatalog() const override { return catalog; }
};

} // namespace db7::catalog