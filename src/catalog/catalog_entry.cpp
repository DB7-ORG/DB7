#include "catalog/entries/catalog_entry.hpp"

namespace db7::catalog {
CatalogEntry::CatalogEntry(CatalogType type, Identifier name_p, idx_t oid)
    : oid(oid), type(type), set(nullptr), name(std::move(name_p)), deleted(false), temporary(false),
      internal(false), parent(nullptr) {}

CatalogEntry::CatalogEntry(CatalogType type, DatabaseCatalog &catalog, Identifier name_p)
    : CatalogEntry(type, std::move(name_p), catalog.NextOid()) {}

CatalogEntry::~CatalogEntry() {}

void CatalogEntry::SetChild(std::unique_ptr<CatalogEntry> child_p) {
  child = std::move(child_p);
  if (child) { child->parent.store(this); }
}

std::unique_ptr<CatalogEntry> CatalogEntry::TakeChild() {
  if (child) { child->parent.store(nullptr); }
  return std::move(child);
}

bool CatalogEntry::HasChild() const { return child != nullptr; }
bool CatalogEntry::HasParent() const { return parent.load() != nullptr; }

CatalogEntry &CatalogEntry::Child() { return *child; }

CatalogEntry &CatalogEntry::Parent() { return *parent.load(); }

const CatalogEntry &CatalogEntry::Parent() const { return *parent.load(); }
} // namespace db7::catalog