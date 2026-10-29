#include "storage/mvcc/page_version_manager.hpp"

namespace db7::storage {
VersionPtr *PageVersionManager::Get(PageIdentifier new_page_id) {
  return partitions_.Get(new_page_id);
}

VersionPtr *PageVersionManager::GetCreateVersions(PageIdentifier id, u32 count) {
  u32 part = partitions_.GetPartitionIdx(id);

  if (auto *v = partitions_.Get(id, part)) return v;

  auto fresh = std::make_unique<VersionPtr[]>(count); // allocate with no lock held

  VersionPtr *existing = nullptr;
  if (partitions_.Put(id, fresh.get(), existing, part))
    return fresh.release(); // we inserted it: hand over ownership

  return existing;
}

storage::UndoRecord *PageVersionManager::GetDelta(TupleId tid, table_id tbl_id) {
  auto *versions = partitions_.Get({tbl_id, tid.GetPageId()});
  if (!versions) return nullptr;
  auto *undo = versions[tid.GetIndex()].Get();
  return undo;
}
} // namespace db7::storage