#include "catalog/catalog_set.hpp"
#include "catalog/database_catalog.hpp"
#include "catalog/dependency/dependency_manager.hpp"
#include "catalog/objects/alter_table_info.hpp"
#include "shared/error/exception.hpp"
#include "shared/macro_helper.hpp"

#include <fmt/format.h>
#include <memory>

namespace db7::catalog {

CatalogSet::CatalogSet(DatabaseCatalog &catalog_p) : catalog(catalog_p) {}
CatalogSet::~CatalogSet() {}

DatabaseCatalog &CatalogEntry::ParentCatalog() {
  throw CATALOG_EXCEPTION("CatalogEntry::ParentCatalog called on catalog entry without catalog");
}

const DatabaseCatalog &CatalogEntry::ParentCatalog() const {
  throw CATALOG_EXCEPTION("CatalogEntry::ParentCatalog called on catalog entry without catalog");
}

SchemaCatalogEntryBase &CatalogEntry::ParentSchema() {
  throw CATALOG_EXCEPTION("CatalogEntry::ParentSchema called on catalog entry without schema");
}

const SchemaCatalogEntryBase &CatalogEntry::ParentSchema() const {
  throw CATALOG_EXCEPTION("CatalogEntry::ParentSchema called on catalog entry without schema");
}

std::unique_ptr<CatalogEntry> CatalogEntry::AlterEntry(transaction::TransactionContext &context,
                                                       AlterInfo &info) {
  throw CATALOG_EXCEPTION("AlterEntry is a virtual method");
}

void CatalogEntryMap::AddEntry(std::unique_ptr<CatalogEntry> entry) {
  auto name = entry->name;

  if (entries.find(name) != entries.end()) {
    throw CATALOG_EXCEPTION(
        fmt::format("Entry with name {} already exists", name.GetIdentifierName()));
  }
  entries.insert(make_pair(name, std::move(entry)));
}

void CatalogEntryMap::UpdateEntry(std::unique_ptr<CatalogEntry> catalog_entry) {
  auto name = catalog_entry->name;

  auto entry = entries.find(name);
  if (entry == entries.end()) {
    throw CATALOG_EXCEPTION(
        fmt::format("Entry with name {} does not exist", name.GetIdentifierName()));
  }

  auto existing = std::move(entry->second);
  entry->second = std::move(catalog_entry);
  entry->second->SetChild(std::move(existing));
}

void CatalogEntryMap::DropEntry(CatalogEntry &entry) {
  auto &name = entry.name;
  auto chain = GetEntry(name);
  if (!chain) {
    throw CATALOG_EXCEPTION(
        fmt::format("Attempting to drop entry with name {} but no chain with that name exists",
                    name.GetIdentifierName()));
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

std::map<Identifier, std::unique_ptr<CatalogEntry>> &CatalogEntryMap::Entries() { return entries; }

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

CatalogEntry &CatalogSet::GetEntryForTransaction(transaction::TransactionContext &context,
                                                 CatalogEntry &current) {
  bool visible;
  return GetEntryForTransaction(context, current, visible);
}

CatalogEntry &CatalogSet::GetEntryForTransaction(transaction::TransactionContext &context,
                                                 CatalogEntry &current, bool &visible) {
  std::reference_wrapper<CatalogEntry> entry(current);
  while (entry.get().HasChild()) {
    if (!transaction::TransactionUtil::HasConflict(entry.get().timestamp, context.FinishTime(),
                                                   context.StartTime())) {
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

void CatalogSet::Scan(transaction::TransactionContext &context,
                      const std::function<void(CatalogEntry &)> &callback) {
  // Lock the catalog set.
  std::unique_lock<std::mutex> lock(catalog_lock);
  // TODO catalog CreateDefaultEntries(context, lock);

  for (auto &kv : map.Entries()) {
    auto &entry = *kv.second;
    auto &entry_for_transaction = GetEntryForTransaction(context, entry);
    if (!entry_for_transaction.deleted) { callback(entry_for_transaction); }
  }
}

void CatalogSet::ScanWithPrefix(transaction::TransactionContext &context,
                                const std::function<void(CatalogEntry &)> &callback,
                                const Identifier &prefix) {
  // lock the catalog set
  std::unique_lock<std::mutex> lock(catalog_lock);
  // TODO catalog CreateDefaultEntries(context, lock);

  auto &entries = map.Entries();
  auto it = entries.lower_bound(prefix);
  auto end = entries.upper_bound(Identifier(prefix.GetIdentifierName() + char(255)));
  for (; it != end; it++) {
    auto &entry = *it->second;
    auto &entry_for_transaction = GetEntryForTransaction(context, entry);
    if (!entry_for_transaction.deleted) { callback(entry_for_transaction); }
  }
}

CatalogSet::EntryLookup CatalogSet::GetEntryDetailed(transaction::TransactionContext &context,
                                                     const Identifier &name) {
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

// static bool IsDependencyEntry(CatalogEntry &entry) {
//   return entry.type == CatalogType::DEPENDENCY_ENTRY;
// }

// TODO catalog here i should make it valid for my design where i dont have catalog per
// database instance
void CatalogSet::CheckCatalogEntryInvariants(CatalogEntry &value, const Identifier &name) {
  // if (value.internal && !catalog.IsSystemCatalog() && name != DEFAULT_SCHEMA) {
  //   throw CATALOG_EXCEPTION(
  //       fmt::format("Attempting to create internal entry {} in non-system catalog - internal "
  //                   "entries can only be created in the system catalog",
  //                   name.GetIdentifierName()));
  // }
  // if (!value.internal) {
  //   if (!value.temporary && catalog.IsSystemCatalog() && !IsDependencyEntry(value)) {
  //     throw CATALOG_EXCEPTION(
  //         fmt::format("Attempting to create non-internal entry {} in system catalog - the system
  //         "
  //                     "catalog can only contain internal entries",
  //                     name.GetIdentifierName()));
  //   }
  //   if (value.temporary && !catalog.IsTemporaryCatalog()) {
  //     throw CATALOG_EXCEPTION(
  //         fmt::format("Attempting to create temporary entry {} in non-temporary catalog",
  //                     name.GetIdentifierName()));
  //   }
  //   if (!value.temporary && catalog.IsTemporaryCatalog() && name != DEFAULT_SCHEMA) {
  //     throw CATALOG_EXCEPTION(fmt::format(
  //         "Cannot create non-temporary entry {} in temporary catalog",
  //         name.GetIdentifierName()));
  //   }
  // }
}

bool CatalogSet::StartChain(transaction::TransactionContext &context, const Identifier &name,
                            std::unique_lock<std::mutex> &read_lock) {
  DB7_ASSERT(!map.GetEntry(name), "");

  // TODO catalog do i need this
  // check if there is a default entry
  // auto entry = CreateDefaultEntry(context, name, read_lock);
  // if (entry) { return false; }

  // first create a dummy deleted entry
  // so other transactions will see that instead of the entry that is to be added.
  auto dummy_node = std::make_unique<InCatalogEntry>(CatalogType::INVALID, catalog, name);
  dummy_node->timestamp = 0;
  dummy_node->deleted = true;
  dummy_node->set = this;

  map.AddEntry(std::move(dummy_node));
  return true;
}

bool CatalogSet::VerifyVacancy(transaction::TransactionContext &context, CatalogEntry &entry) {
  if (transaction::TransactionUtil::HasConflict(entry.timestamp, context.FinishTime(),
                                                context.StartTime())) {
    // A transaction that is not visible to our snapshot has already made a change to this entry.
    // Because of Catalog limitations we can't push our change on this, even if the change was made
    // by another active transaction that might end up being aborted. So we have to cancel this
    // transaction.
    throw CATALOG_EXCEPTION("Catalog write-write conflict on create with " + entry.name);
  }
  // The entry is visible to our snapshot
  if (!entry.deleted) { return false; }
  return true;
}

bool CatalogSet::CreateEntryInternal(transaction::TransactionContext &context,
                                     const Identifier &name, std::unique_ptr<CatalogEntry> value,
                                     std::unique_lock<std::mutex> &read_lock,
                                     bool should_be_empty) {
  auto entry_value = map.GetEntry(name);
  if (!entry_value) {
    // Add a dummy node to start the chain
    if (!StartChain(context, name, read_lock)) { return false; }
  } else if (should_be_empty) {
    // Verify that the entry is deleted, not altered by another transaction
    if (!VerifyVacancy(context, *entry_value)) { return false; }
  }

  // Finally add the new entry to the chain
  // auto value_ptr = value.get();
  map.UpdateEntry(std::move(value));
  // Push the old entry in the undo buffer for this transaction, so it can be restored in the event
  // of failure
  // TODO catalog duck db has to do this because its in memory so u need seperate txn system to
  // track this i could reuse disk mvcc so this should be done differerntly if
  // (transaction.transaction) {
  //   DuckTransactionManager::Get(GetCatalog().GetAttached())
  //       .PushCatalogEntry(*transaction.transaction, value_ptr->Child());
  // }
  return true;
}

optional_ptr<CatalogEntry> CatalogSet::GetEntry(transaction::TransactionContext &context,
                                                const Identifier &name) {
  auto lookup = GetEntryDetailed(context, name);
  return lookup.result;
}

bool CatalogSet::CreateEntry(transaction::TransactionContext &context, const Identifier &name,
                             std::unique_ptr<CatalogEntry> value,
                             const LogicalDependencyList &dependencies) {
  CheckCatalogEntryInvariants(*value, name);

  // Mark this entry as being created by the current active transaction
  value->timestamp = context.FinishTime();
  value->set = this;
  catalog.GetDependencyManager()->AddObject(context, *value, dependencies);

  // lock the catalog for writing
  std::lock_guard<std::mutex> write_lock(catalog.GetLock());
  // lock this catalog set to disallow reading
  std::unique_lock<std::mutex> read_lock(catalog_lock);

  return CreateEntryInternal(context, name, std::move(value), read_lock);
}

bool CatalogSet::DropDependencies(transaction::TransactionContext &context, const Identifier &name,
                                  bool cascade, bool allow_drop_internal) {
  auto entry = GetEntry(context, name);
  if (!entry) { return false; }
  if (entry->internal && !allow_drop_internal) {
    throw CATALOG_EXCEPTION(
        fmt::format("Cannot drop entry {} because it is an internal system entry",
                    entry->name.GetIdentifierName()));
  }
  // check any dependencies of this object
  catalog.GetDependencyManager()->DropObject(context, *entry, cascade);
  return true;
}

//! This method is used to retrieve an entry for the purpose of making a new version, through an
//! alter/drop/create
optional_ptr<CatalogEntry> CatalogSet::GetEntryInternal(transaction::TransactionContext &context,
                                                        const Identifier &name) {
  auto entry_value = map.GetEntry(name);
  if (!entry_value) { return nullptr; }
  auto &catalog_entry = *entry_value;

  // Check if this entry is visible to our snapshot
  if (transaction::TransactionUtil::HasConflict(catalog_entry.timestamp, context.FinishTime(),
                                                context.StartTime())) {
    // We intend to create a new version of the entry.
    // Another transaction has already made an edit to this catalog entry, because of limitations in
    // the Catalog we can't create an edit alongside this even if the other transaction might end up
    // getting aborted. So we have to abort the transaction.
    throw CATALOG_EXCEPTION(fmt::format("Catalog write-write conflict on alter with {}",
                                        catalog_entry.name.GetIdentifierName()));
  }
  // The entry is visible to our snapshot, check if it's deleted
  if (catalog_entry.deleted) { return nullptr; }
  return &catalog_entry;
}

bool CatalogSet::DropEntryInternal(transaction::TransactionContext &context, const Identifier &name,
                                   bool allow_drop_internal) {
  // lock the catalog for writing
  // we can only delete an entry that exists
  auto entry = GetEntryInternal(context, name);
  if (!entry) { return false; }
  if (entry->internal && !allow_drop_internal) {
    throw CATALOG_EXCEPTION(
        fmt::format("Cannot drop entry {} because it is an internal system entry",
                    entry->name.GetIdentifierName()));
  }

  // create a new tombstone entry and replace the currently stored one
  // set the timestamp to the timestamp of the current transaction
  // and point it at the tombstone node
  auto value = std::make_unique<InCatalogEntry>(CatalogType::DELETED_ENTRY, entry->ParentCatalog(),
                                                entry->name);
  value->timestamp = context.FinishTime();
  value->set = this;
  value->deleted = true;
  // auto value_ptr = value.get();
  map.UpdateEntry(std::move(value));

  // push the old entry in the undo buffer for this transaction
  // TODO catalog duck db has to do this because its in memory so u need seperate txn system to
  // track this i could reuse disk mvcc so this should be done differerntly if
  // if (transaction.transaction) {
  //   DuckTransactionManager::Get(GetCatalog().GetAttached())
  //       .PushCatalogEntry(*transaction.transaction, value_ptr->Child());
  // }
  return true;
}

bool CatalogSet::DropEntry(transaction::TransactionContext &context, const Identifier &name,
                           bool cascade, bool allow_drop_internal) {
  if (!DropDependencies(context, name, cascade, allow_drop_internal)) { return false; }
  std::lock_guard<std::mutex> write_lock(catalog.GetLock());
  std::lock_guard<std::mutex> read_lock(catalog_lock);
  return DropEntryInternal(context, name, allow_drop_internal);
}

bool CatalogSet::RenameEntryInternal(transaction::TransactionContext &context, CatalogEntry &old,
                                     const Identifier &new_name, AlterInfo &alter_info,
                                     std::unique_lock<std::mutex> &read_lock) {
  auto &original_name = old.name;

  auto entry_value = map.GetEntry(new_name);
  if (entry_value) {
    auto &existing_entry = GetEntryForTransaction(context, *entry_value);
    if (!existing_entry.deleted) {
      // There exists an entry by this name that is not deleted
      throw CATALOG_EXCEPTION(
          fmt::format("Could not rename {} to {}: another entry with this name already exists!",
                      original_name.GetIdentifierName(), new_name.GetIdentifierName()));
    }
  }

  // Add a RENAMED_ENTRY before adding a DELETED_ENTRY, this makes it so that when this is committed
  // we know that this was not a DROP statement.
  auto renamed_tombstone = std::make_unique<InCatalogEntry>(CatalogType::RENAMED_ENTRY,
                                                            old.ParentCatalog(), original_name);
  renamed_tombstone->timestamp = context.FinishTime();
  renamed_tombstone->deleted = false;
  renamed_tombstone->set = this;
  if (!CreateEntryInternal(context, original_name, std::move(renamed_tombstone), read_lock,
                           /*should_be_empty = */ false)) {
    return false;
  }
  if (!DropEntryInternal(context, original_name, false)) { return false; }

  // Add the renamed entry
  // Start this off with a RENAMED_ENTRY node, for commit/cleanup/rollback purposes
  auto renamed_node =
      std::make_unique<InCatalogEntry>(CatalogType::RENAMED_ENTRY, catalog, new_name);
  renamed_node->timestamp = context.FinishTime();
  renamed_node->deleted = false;
  renamed_node->set = this;
  return CreateEntryInternal(context, new_name, std::move(renamed_node), read_lock);
}

bool CatalogSet::AlterEntry(transaction::TransactionContext &context, const Identifier &name,
                            AlterInfo &alter_info) {
  // If the entry does not exist, we error
  auto entry = GetEntry(context, name);
  if (!entry) { return false; }
  // internal modifications are not allowed
  if (!alter_info.allow_internal && entry->internal) {
    throw CATALOG_EXCEPTION(
        fmt::format("Cannot alter entry {} because it is an internal system entry",
                    entry->name.GetIdentifierName()));
  }

  std::unique_ptr<CatalogEntry> value;
  // Here we created Copy of the original entry with modifications
  value = entry->AlterEntry(context, alter_info);
  if (!value) {
    // alter failed, but did not result in an error
    return true;
  }

  // lock the catalog for writing
  std::unique_lock<std::mutex> write_lock(catalog.GetLock());
  // lock this catalog set to disallow reading
  std::unique_lock<std::mutex> read_lock(catalog_lock);

  // fetch the entry again before doing the modification
  // this will catch any write-write conflicts between transactions
  entry = GetEntryInternal(context, name);

  // Mark this entry as being created by this transaction
  value->timestamp = context.FinishTime();
  value->set = this;

  if (!(value->name == entry->name)) {
    if (!RenameEntryInternal(context, *entry, value->name, alter_info, read_lock)) { return false; }
  }
  auto new_entry = value.get();
  map.UpdateEntry(std::move(value));

  // TODO catalog
  //  push the old entry in the undo buffer for this transaction
  //  std::unique_ptr<CatalogEntry> entry_to_destroy;
  //  if (transaction.transaction) {
  //    // serialize the AlterInfo into a temporary buffer
  //    MemoryStream stream(Allocator::Get(*transaction.db));
  //    BinarySerializer serializer(stream);
  //    serializer.Begin();
  //    serializer.WriteProperty(100, "column_name", alter_info.GetColumnName());
  //    serializer.WriteProperty(101, "alter_info", &alter_info);
  //    serializer.End();

  //   DuckTransactionManager::Get(GetCatalog().GetAttached())
  //       .PushCatalogEntry(*transaction.transaction, new_entry->Child(), stream.GetData(),
  //                         stream.GetPosition());
  // } else {
  //   // if we don't have a transaction this alter is non-transactional
  //   // in that case we are able to just directly destroy the child (if there is any)
  //   entry_to_destroy = new_entry->TakeChild();
  // }

  read_lock.unlock();
  write_lock.unlock();

  // Check the dependency manager to verify that there are no conflicting dependencies with this
  // alter
  catalog.GetDependencyManager()->AlterObject(context, *entry, *new_entry, alter_info);
  return true;
}

} // namespace db7::catalog