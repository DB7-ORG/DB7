#include "catalog/catalog_set.hpp"
#include "shared/error/exception.hpp"

namespace db7::catalog {

void CatalogEntryMap::AddEntry(std::unique_ptr<CatalogEntry> entry) {
  auto name = entry->name;

  if (entries.find(name) != entries.end()) {
    throw CATALOG_EXCEPTION(fmt::format("Entry with name {} already exists", name));
  }
  entries.insert(make_pair(name, std::move(entry)));
}

void CatalogEntryMap::UpdateEntry(std::unique_ptr<CatalogEntry> catalog_entry) {
  auto name = catalog_entry->name;

  auto entry = entries.find(name);
  if (entry == entries.end()) {
    throw CATALOG_EXCEPTION(fmt::format("Entry with name {} does not exist", name));
  }

  auto existing = std::move(entry->second);
  entry->second = std::move(catalog_entry);
  entry->second->SetChild(std::move(existing));
}

void CatalogEntryMap::DropEntry(CatalogEntry &entry) {
  auto &name = entry.name;
  auto chain = GetEntry(name);
  if (!chain) {
    throw CATALOG_EXCEPTION(fmt::format(
        "Attempting to drop entry with name {} but no chain with that name exists", name));
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

optional_ptr<CatalogEntry> CatalogEntryMap::GetEntry(const std::string &name) {
  auto entry = entries.find(name);
  if (entry == entries.end()) { return nullptr; }
  return entry->second.get();
}
} // namespace db7::catalog