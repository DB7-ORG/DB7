#pragma once

#include "catalog/catalog_common.hpp"
#include "shared/pointers/optional_ptr.hpp"
#include "transaction/transaction_common.hpp"

#include <atomic>
#include <string>

namespace db7 {

class Catalog;
class CatalogSet;

namespace catalog {

class CatalogEntry {
public:
  //! The oid of the entry
  idx_t oid;
  //! The type of this catalog entry
  CatalogType type;
  //! Reference to the catalog set this entry is stored in
  optional_ptr<CatalogSet> set;
  //! The name of the entry
  std::string name;
  //! Whether or not the object is deleted
  bool deleted;
  //! Whether or not the object is temporary and should not be added to the WAL
  bool temporary;
  //! Whether or not the entry is an internal entry (cannot be deleted, not dumped, etc)
  bool internal;
  //! The name of the extension that registered this entry (empty for core entries)
  std::string extension_name;
  //! Timestamp at which the catalog entry was created
  std::atomic<timestamp_t> timestamp;

private:
  //! Child entry
  std::unique_ptr<CatalogEntry> child;
  //! Parent entry (the node that dependents_map this node)
  std::atomic<CatalogEntry *> parent;

public:
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
} // namespace catalog
} // namespace db7