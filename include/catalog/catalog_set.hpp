#pragma once

#include "catalog/entries/catalog_entry.hpp"
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
  CatalogEntry &GetCommittedEntry(CatalogEntry &current);
  CatalogEntry &GetEntryForTransaction(transaction::TransactionContext &context,
                                       CatalogEntry &current);
  CatalogEntry &GetEntryForTransaction(transaction::TransactionContext &context,
                                       CatalogEntry &current, bool &visible);

  void CheckCatalogEntryInvariants(CatalogEntry &value, const Identifier &name);
  bool StartChain(transaction::TransactionContext &context, const Identifier &name,
                  std::unique_lock<std::mutex> &read_lock);
  bool VerifyVacancy(transaction::TransactionContext &context, CatalogEntry &entry);
  bool CreateEntryInternal(transaction::TransactionContext &context, const Identifier &name,
                           std::unique_ptr<CatalogEntry> value,
                           std::unique_lock<std::mutex> &read_lock, bool should_be_empty = true);
  bool DropDependencies(transaction::TransactionContext &context, const Identifier &name,
                        bool cascade, bool allow_drop_internal);
  bool DropEntryInternal(transaction::TransactionContext &context, const Identifier &name,
                         bool allow_drop_internal);
  optional_ptr<CatalogEntry> GetEntryInternal(transaction::TransactionContext &context,
                                              const Identifier &name);

public:
  explicit CatalogSet(DatabaseCatalog &catalog);
  ~CatalogSet();

  bool CreateEntry(transaction::TransactionContext &context, const Identifier &name,
                   std::unique_ptr<CatalogEntry> value, const LogicalDependencyList &dependencies);
  bool DropEntry(transaction::TransactionContext &context, const Identifier &name, bool cascade,
                 bool allow_drop_internal = false);
  CatalogSet::EntryLookup GetEntryDetailed(transaction::TransactionContext &context,
                                           const Identifier &name);

  void Scan(const std::function<void(CatalogEntry &)> &callback);
  void Scan(transaction::TransactionContext &context,
            const std::function<void(CatalogEntry &)> &callback);
  void ScanWithPrefix(transaction::TransactionContext &context,
                      const std::function<void(CatalogEntry &)> &callback,
                      const Identifier &prefix);

  optional_ptr<CatalogEntry> GetEntry(transaction::TransactionContext &context,
                                      const Identifier &name);
};

} // namespace db7::catalog