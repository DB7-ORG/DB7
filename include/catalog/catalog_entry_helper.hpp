#pragma once

#include "common.hpp"
#include "shared/helper.hpp"

#include <functional>
#include <unordered_set>

namespace db7::catalog {
class CatalogEntry;

struct CatalogEntryHashFunction {
  u64 operator()(const std::reference_wrapper<CatalogEntry> &a) const {
    std::hash<void *> hash_func;
    return hash_func((void *)&a.get());
  }
};

struct CatalogEntryEquality {
  bool operator()(const std::reference_wrapper<CatalogEntry> &a, const std::reference_wrapper<CatalogEntry> &b) const {
    return RefersToSameObject(a, b);
  }
};

using catalog_entry_set_t = std::unordered_set<std::reference_wrapper<CatalogEntry>, CatalogEntryHashFunction, CatalogEntryEquality>;

template <typename T>
using catalog_entry_map_t = std::unordered_map<std::reference_wrapper<CatalogEntry>, T, CatalogEntryHashFunction, CatalogEntryEquality>;

using catalog_entry_vector_t = std::vector<std::reference_wrapper<CatalogEntry>>;

} // namespace db7::catalog