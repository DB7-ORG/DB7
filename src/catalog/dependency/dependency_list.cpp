#include "catalog/dependency/dependency_list.hpp"
#include "catalog/entries/dependency_entry.hpp"
#include "catalog/entries/schema_catalog_entry.hpp"
#include "shared/hash_util.hpp"
#include "shared/identifier.hpp"

namespace db7::catalog {

static std::string GetSchema(CatalogEntry &entry) {
  if (entry.type == CatalogType::SCHEMA_ENTRY) { return entry.name.GetIdentifierName(); }
  return entry.ParentSchema().name.GetIdentifierName();
}

LogicalDependency::LogicalDependency(CatalogEntry &entry) {
  catalog = Identifier::InvalidCatalog();
  if (entry.type == CatalogType::DEPENDENCY_ENTRY) {
    auto &dependency_entry = entry.Cast<DependencyEntry>();

    this->entry = dependency_entry.EntryInfo();
  } else {
    this->entry.schema = Identifier(GetSchema(entry));
    this->entry.name = entry.name;
    this->entry.type = entry.type;
    catalog = entry.ParentCatalog().GetName();
  }
}

LogicalDependency::LogicalDependency(optional_ptr<DatabaseCatalog> catalog_p,
                                     CatalogEntryInfo entry_p, Identifier catalog_str)
    : entry(std::move(entry_p)), catalog(std::move(catalog_str)) {
  if (catalog_p) { catalog = catalog_p->GetName(); }
}

bool LogicalDependency::operator==(const LogicalDependency &other) const {
  return other.entry.name == entry.name && other.entry.schema == entry.schema &&
         other.entry.type == entry.type;
}

uint64_t LogicalDependencyHashFunction::operator()(const LogicalDependency &a) const {
  auto &name = a.entry.name;
  auto &schema = a.entry.schema;
  auto &type = a.entry.type;
  auto &catalog = a.catalog;

  hash_t hash = HashUtil::Hash(name.c_str());
  hash = HashUtil::CombineHash(hash, HashUtil::Hash(schema.c_str()));
  hash = HashUtil::CombineHash(hash, HashUtil::Hash(catalog.c_str()));
  hash = HashUtil::CombineHash(hash, HashUtil::Hash<u8>(static_cast<u8>(type)));
  return hash;
}

bool LogicalDependencyEquality::operator()(const LogicalDependency &a,
                                           const LogicalDependency &b) const {
  if (a.entry.type != b.entry.type) { return false; }
  if (a.entry.name != b.entry.name) { return false; }
  if (a.entry.schema != b.entry.schema) { return false; }
  if (a.catalog != b.catalog) { return false; }
  return true;
}

void LogicalDependencyList::AddDependency(CatalogEntry &entry) {
  LogicalDependency dependency(entry);
  set.insert(dependency);
}

void LogicalDependencyList::AddDependency(const LogicalDependency &entry) { set.insert(entry); }

bool LogicalDependencyList::Contains(CatalogEntry &entry_p) {
  LogicalDependency logical_entry(entry_p);
  return set.count(logical_entry);
}
} // namespace db7::catalog