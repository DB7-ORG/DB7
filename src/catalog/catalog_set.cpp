#include "catalog/catalog_set.hpp"
#include "shared/error/exception.hpp"
#include "shared/macro_helper.hpp"

#include <fmt/format.h>

namespace db7::catalog {

CatalogSet::CatalogSet(DatabaseCatalog &catalog_p) : catalog(catalog_p) {}
CatalogSet::~CatalogSet() {}

DatabaseCatalog &CatalogEntry::ParentCatalog() { throw CATALOG_EXCEPTION("CatalogEntry::ParentCatalog called on catalog entry without catalog"); }

const DatabaseCatalog &CatalogEntry::ParentCatalog() const {
  throw CATALOG_EXCEPTION("CatalogEntry::ParentCatalog called on catalog entry without catalog");
}

SchemaCatalogEntry &CatalogEntry::ParentSchema() { throw CATALOG_EXCEPTION("CatalogEntry::ParentSchema called on catalog entry without schema"); }

const SchemaCatalogEntry &CatalogEntry::ParentSchema() const {
  throw CATALOG_EXCEPTION("CatalogEntry::ParentSchema called on catalog entry without schema");
}

void CatalogEntryMap::AddEntry(std::unique_ptr<CatalogEntry> entry) {
  auto name = entry->name;

  if (entries.find(name) != entries.end()) { throw CATALOG_EXCEPTION(fmt::format("Entry with name {} already exists", name.GetIdentifierName())); }
  entries.insert(make_pair(name, std::move(entry)));
}

void CatalogEntryMap::UpdateEntry(std::unique_ptr<CatalogEntry> catalog_entry) {
  auto name = catalog_entry->name;

  auto entry = entries.find(name);
  if (entry == entries.end()) { throw CATALOG_EXCEPTION(fmt::format("Entry with name {} does not exist", name.GetIdentifierName())); }

  auto existing = std::move(entry->second);
  entry->second = std::move(catalog_entry);
  entry->second->SetChild(std::move(existing));
}

void CatalogEntryMap::DropEntry(CatalogEntry &entry) {
  auto &name = entry.name;
  auto chain = GetEntry(name);
  if (!chain) {
    throw CATALOG_EXCEPTION(fmt::format("Attempting to drop entry with name {} but no chain with that name exists", name.GetIdentifierName()));
  }
  auto child = entry.TakeChild();
  if (!entry.HasParent()) {
    // This is the top of the chain
    DB7_ASSERT(chain.get() == &entry, "Should be the top of the chain");
    auto it = entries.find(name);
    DB7_ASSERT(it != entries.end(), "Should be the top of the chain");

    // Remove the entry
    it->second.reset();
    if (child) {
      // Replace it with its child
      it->second = std::move(child);
    } else {
      entries.erase(it);
    }
  } else {
    // Just replace the entry with its child
    auto &parent = entry.Parent();
    parent.SetChild(std::move(child));
  }
}

std::unordered_map<Identifier, std::unique_ptr<CatalogEntry>> &CatalogEntryMap::Entries() { return entries; }

optional_ptr<CatalogEntry> CatalogEntryMap::GetEntry(const Identifier &name) {
  auto entry = entries.find(name);
  if (entry == entries.end()) { return nullptr; }
  return entry->second.get();
}

CatalogEntry &CatalogSet::GetCommittedEntry(CatalogEntry &current) {
  std::reference_wrapper<CatalogEntry> entry(current);
  while (entry.get().HasChild()) {
    if (transaction::TransactionUtil::IsCommitted(entry.get().timestamp)) { break; }
    entry = entry.get().Child();
  }
  return entry.get();
}

CatalogEntry &CatalogSet::GetEntryForTransaction(transaction::TransactionContext &context, CatalogEntry &current) {
  bool visible;
  return GetEntryForTransaction(context, current, visible);
}

CatalogEntry &CatalogSet::GetEntryForTransaction(transaction::TransactionContext &context, CatalogEntry &current, bool &visible) {
  std::reference_wrapper<CatalogEntry> entry(current);
  while (entry.get().HasChild()) {
    if (transaction::TransactionUtil::HasConflict(entry.get().timestamp, context.FinishTime(), context.StartTime())) {
      visible = true;
      return entry.get();
    }
    entry = entry.get().Child();
  }
  visible = false;
  return entry.get();
}

void CatalogSet::Scan(const std::function<void(CatalogEntry &)> &callback) {
  // Lock the catalog set.
  std::unique_lock<std::mutex> lock(catalog_lock);
  for (auto &kv : map.Entries()) {
    auto &entry = *kv.second;
    auto &committed_entry = GetCommittedEntry(entry);
    if (!committed_entry.deleted) { callback(committed_entry); }
  }
}

void CatalogSet::Scan(transaction::TransactionContext &context, const std::function<void(CatalogEntry &)> &callback) {
  // Lock the catalog set.
  std::unique_lock<std::mutex> lock(catalog_lock);
  // TODO catalog CreateDefaultEntries(context, lock);

  for (auto &kv : map.Entries()) {
    auto &entry = *kv.second;
    auto &entry_for_transaction = GetEntryForTransaction(context, entry);
    if (!entry_for_transaction.deleted) { callback(entry_for_transaction); }
  }
}

CatalogSet::EntryLookup CatalogSet::GetEntryDetailed(transaction::TransactionContext &context, const Identifier &name) {
  std::unique_lock<std::mutex> read_lock(catalog_lock);
  auto entry_value = map.GetEntry(name);
  if (entry_value) {
    // we found an entry for this name
    // check the version numbers

    auto &catalog_entry = *entry_value;
    bool visible;
    auto &current = GetEntryForTransaction(context, catalog_entry, visible);
    if (current.deleted) {
      if (!visible) {
        return EntryLookup{nullptr, EntryLookup::FailureReason::INVISIBLE};
      } else {
        return EntryLookup{nullptr, EntryLookup::FailureReason::DELETED};
      }
    }
    DB7_ASSERT(current.name == name, "eq");
    return EntryLookup{&current, EntryLookup::FailureReason::SUCCESS};
  }
  return EntryLookup{nullptr, EntryLookup::FailureReason::NOT_PRESENT};
}

optional_ptr<CatalogEntry> CatalogSet::GetEntry(transaction::TransactionContext &context, const Identifier &name) {
  auto lookup = GetEntryDetailed(context, name);
  return lookup.result;
}

} // namespace db7::catalog