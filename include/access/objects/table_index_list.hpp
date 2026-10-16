#pragma once

#include "access/objects/add_table_index_info.hpp"

#include <mutex>
#include <shared_mutex>
#include <vector>

namespace db7::access {

class TableIndexList {
private:
  mutable std::shared_mutex index_entries_lock;
  std::vector<std::unique_ptr<AddTableIndexInfo>> index_entries;

public:
  std::vector<AddTableIndexInfoView> EntriesView() const {
    std::shared_lock _(index_entries_lock);
    std::vector<AddTableIndexInfoView> result;
    result.reserve(index_entries.size());
    for (const auto &index : index_entries) { result.emplace_back(index->View()); }
    return result;
  }

  void AddIndex(std::unique_ptr<AddTableIndexInfo> info) {
    std::unique_lock _(index_entries_lock);
    index_entries.push_back(std::move(info));
  }
};

} // namespace db7::access