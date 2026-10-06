#pragma once

#include "catalog/entries/catalog_entry.hpp"
#include "catalog/objects/alter_table_info.hpp"
#include "dependency/dependency_list.hpp"
#include "shared/identifier.hpp"
#include "transaction/transaction_context.hpp"

#include <unordered_map>

namespace db7::catalog {

class DatabaseCatalog;
class LogicalDependencyList;

class CatalogEntryMap {

private:
  //! Mapping of identifier to catalog entry
  std::map<Identifier, std::unique_ptr<CatalogEntry>> entries;

public:
  CatalogEntryMap() {}

  void AddEntry(std::unique_ptr<CatalogEntry> entry);
  void UpdateEntry(std::unique_ptr<CatalogEntry> entry);
  void DropEntry(CatalogEntry &entry);
  std::map<Identifier, std::unique_ptr<CatalogEntry>> &Entries();
  optional_ptr<CatalogEntry> GetEntry(const Identifier &name);
};

class CatalogSet {
private:
  std::mutex catalog_lock;
  CatalogEntryMap map;
  DatabaseCatalog &catalog;

  //! The generator used to generate default internal entries
  // unique_ptr<DefaultGenerator> defaults;
public:
  struct EntryLookup {
    enum class FailureReason { SUCCESS, DELETED, NOT_PRESENT, INVISIBLE };
    optional_ptr<CatalogEntry> result;
    FailureReason reason;
  };

private:
  // Get commited version of the entry
  CatalogEntry &GetCommittedEntry(CatalogEntry &current);

  // Get entry for this transaction context
  CatalogEntry &GetEntryForTransaction(transaction::TransactionContext &context,
                                       CatalogEntry &current);
  CatalogEntry &GetEntryForTransaction(transaction::TransactionContext &context,
                                       CatalogEntry &current, bool &visible);

  // TODO catalog do i need this
  void CheckCatalogEntryInvariants(CatalogEntry &value, const Identifier &name);

  // Creates a dummy node and places it in the set
  bool StartChain(transaction::TransactionContext &context, const Identifier &name,
                  std::unique_lock<std::mutex> &read_lock);

  // Validates mvcc correctness (there is no conflict with another txn)
  bool VerifyVacancy(transaction::TransactionContext &context, CatalogEntry &entry);

  /**
   * This method is used to retrieve an entry for the purpose of making a new version, through an
   * alter/drop/create
   * @param name entry identifier
   */
  optional_ptr<CatalogEntry> GetEntryInternal(transaction::TransactionContext &context,
                                              const Identifier &name);

  /**
   * Creates entry in the set
   * @param name                entry identifier
   * @param value               entry ptr
   * @param read_lock,should_be_empty not sure if i need this // TODO catalog
   */
  bool CreateEntryInternal(transaction::TransactionContext &context, const Identifier &name,
                           std::unique_ptr<CatalogEntry> value,
                           std::unique_lock<std::mutex> &read_lock, bool should_be_empty = true);

  /**
   * Used to drop all dependencies for a given entry. It scans dependents to check none of them are
   * blocking and if cascade it drops them also. Also scans subjects for stuff like sequences where
   * ownership flag is set.
   * @param name                entry identifier
   * @param cascade             drop dependencies that are associated with the entry
   * @param allow_drop_internal is dropping internal tables allowed
   */
  bool DropDependencies(transaction::TransactionContext &context, const Identifier &name,
                        bool cascade, bool allow_drop_internal);

  /**
   * Using GetEntryEnternal getches the entry and places deleted tombstone in front marking it
   * invisible for other transactions
   * @param name                entry identifier
   * @param allow_drop_internal is dropping internal tables allowed
   */
  bool DropEntryInternal(transaction::TransactionContext &context, const Identifier &name,
                         bool allow_drop_internal);

public:
  explicit CatalogSet(DatabaseCatalog &catalog);
  ~CatalogSet();

  /**
   * Inserts entry to CatalogSet while holding lock
   * @param name          entry identifier
   * @param value         entry that is inserted
   * @param dependencies  objects this entry depends on
   */
  bool CreateEntry(transaction::TransactionContext &context, const Identifier &name,
                   std::unique_ptr<CatalogEntry> value, const LogicalDependencyList &dependencies);

  /**
   * Used for dropping an entry from CatalogSet while holding lock
   * @param name                  entry identifier
   * @param cascade               should it cascade drop entries that depend on it
   * @param allow_drop_internal   allow dropping internal (system) entries
   */
  bool DropEntry(transaction::TransactionContext &context, const Identifier &name, bool cascade,
                 bool allow_drop_internal = false);

  bool AlterEntry(transaction::TransactionContext &context, const Identifier &name,
                  AlterInfo &alter_info);
  /**
   * Get entry for a current transaction, and return apropriate error if not found
   * @param name entry identifier
   */
  CatalogSet::EntryLookup GetEntryDetailed(transaction::TransactionContext &context,
                                           const Identifier &name);

  /**
   * Scan over commited versions of entries
   * @param callback function invoked for every entry
   */
  void Scan(const std::function<void(CatalogEntry &)> &callback);

  /**
   * Scan over valid versions of entries for a current transaction
   * @param callback function invoked for every entry
   */
  void Scan(transaction::TransactionContext &context,
            const std::function<void(CatalogEntry &)> &callback);

  /**
   * Scan over valid versions of entries that start with prefix for a current transaction
   * @param callback function invoked for every entry
   * @param prefix   prefix for entries
   */
  void ScanWithPrefix(transaction::TransactionContext &context,
                      const std::function<void(CatalogEntry &)> &callback,
                      const Identifier &prefix);

  /**
   * Get entry for a current transaction
   * @param name entry identifier
   */
  optional_ptr<CatalogEntry> GetEntry(transaction::TransactionContext &context,
                                      const Identifier &name);

  bool RenameEntryInternal(transaction::TransactionContext &context, CatalogEntry &old,
                           const Identifier &new_name, AlterInfo &alter_info,
                           std::unique_lock<std::mutex> &read_lock);
};

} // namespace db7::catalog