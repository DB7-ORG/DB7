#pragma once

#include "shared/models/tuple_id.hpp"
#include "storage/buffer_pool/buffer_partitions.hpp"
#include "storage/mvcc/version_ptr.hpp"
#include "storage/storage_common.hpp"

namespace db7::storage {
class PageVersionManager {
private:
  static constexpr u32 PageVersionPartitions = BUFFER_POOL_PARTITION_NUM;

private:
  using PartitionedMap = BufferPartitions<PageIdentifier, VersionPtr *, PageVersionPartitions,
                                          std::default_delete<VersionPtr[]>>;
  PartitionedMap partitions_;

public:
  PageVersionManager() = default;
  ~PageVersionManager() = default;

  VersionPtr *Get(PageIdentifier new_page_id);

  VersionPtr *GetCreateVersions(PageIdentifier id, u32 count);

  storage::UndoRecord *GetDelta(TupleId tid, table_id tbl_id);
};
} // namespace db7::storage