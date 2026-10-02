#pragma once

#include "catalog/entries/catalog_entry.hpp"
#include "shared/identifier.hpp"
#include "transaction/transaction_context.hpp"

#include <unordered_map>

namespace db7::catalog {

class DatabaseCatalog;

class CatalogEntryMap {

private:
  //! Mapping of identifier to catalog entry
  std::unordered_map<Identifier, std::unique_ptr<CatalogEntry>> entries;

public:
  CatalogEntryMap() {}

  void AddEntry(std::unique_ptr<CatalogEntry> entry);
  void UpdateEntry(std::unique_ptr<CatalogEntry> entry);
  void DropEntry(CatalogEntry &entry);
  std::unordered_map<Identifier, std::unique_ptr<CatalogEntry>> &Entries();
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
  CatalogEntry &GetEntryForTransaction(transaction::TransactionContext &context, CatalogEntry &current);
  CatalogEntry &GetEntryForTransaction(transaction::TransactionContext &context, CatalogEntry &current, bool &visible);
  CatalogSet::EntryLookup GetEntryDetailed(transaction::TransactionContext &context, const Identifier &name);

public:
  explicit CatalogSet(DatabaseCatalog &catalog);
  ~CatalogSet();

  void Scan(const std::function<void(CatalogEntry &)> &callback);
  void Scan(transaction::TransactionContext &context, const std::function<void(CatalogEntry &)> &callback);

  optional_ptr<CatalogEntry> GetEntry(transaction::TransactionContext &context, const Identifier &name);
};

} // namespace db7::catalog