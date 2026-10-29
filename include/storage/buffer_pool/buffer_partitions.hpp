#pragma once

#include "shared/hash_util.hpp"
#include "shared/locks/adaptive_version_lock.hpp"
#include "storage/storage_common.hpp"
#include "third_party/unordered_dense.h"

namespace db7::storage {

template <typename Key, typename Val, int PCount, typename Deleter = void>
class BufferPartitions {
  static_assert(PCount > 0 && (PCount & (PCount - 1)) == 0, "PCount must be a power of 2");

private:
  struct alignas(64) AlignedLock {
    shared::AdaptiveVersionLock lock;
  };
  using Map = ankerl::unordered_dense::map<Key, Val>;
  Map partition_maps_[PCount];
  AlignedLock partition_locks_[PCount];

  static constexpr Val NotFound() {
    if constexpr (std::is_pointer_v<Val>)
      return nullptr;
    else if constexpr (std::is_integral_v<Val>)
      return std::numeric_limits<Val>::max();
    else
      return Val{}; // e.g. FrameRef, whose default is the sentinel
  }

  Val GetUnsafe(Map &partition_map, Key id) {
    auto it = partition_map.find(id);
    if (it != partition_map.end()) { return it->second; }
    return NotFound();
  }

public:
  ~BufferPartitions() {
    if constexpr (!std::is_void_v<Deleter>) {
      for (auto &map : partition_maps_)
        for (auto &[key, val] : map) Deleter{}(val);
    }
  }

  bool IsValid(Val value) const { return value != NotFound(); }

  u32 GetPartitionIdx(Key id) { return HashUtil::murmurhash64(id.packed) % PCount; }

  void Reserve(size_t n, u32 partIdx) {
    shared::AdaptiveVersionLock::WriteGuard guard(partition_locks_[partIdx].lock);
    partition_maps_[partIdx].reserve(n);
  }

  Val Get(Key id) {
    auto part = GetPartitionIdx(id);
    return Get(id, part);
  }

  Val Get(Key id, u32 partIdx) {
    auto &partition = partition_maps_[partIdx];
    auto *lock = &partition_locks_[partIdx].lock;
    shared::AdaptiveVersionLock::ReadGuard guard(*lock);
    return GetUnsafe(partition, id);
  };

  bool Put(Key id, Val val, Val &new_val) {
    auto part = GetPartitionIdx(id);
    return Put(id, val, new_val, part);
  }

  bool Put(Key id, Val val, Val &new_val, u32 partIdx) {
    auto &partition = partition_maps_[partIdx];
    shared::AdaptiveVersionLock::WriteGuard guard(partition_locks_[partIdx].lock);
    auto [it, inserted] = partition.emplace(id, val);
    new_val = it->second;
    return inserted;
  }

  void Delete(PageIdentifier id, Val old_val) {
    auto part = GetPartitionIdx(id);
    return Delete(id, old_val, part);
  }

  void Delete(PageIdentifier id, Val old_val, u32 partIdx) {
    auto &partition = partition_maps_[partIdx];
    shared::AdaptiveVersionLock::WriteGuard guard(partition_locks_[partIdx].lock);
    auto it = partition.find(id);
    if (it != partition.end() && it->second == old_val) { partition.erase(it); }
  }
};
} // namespace db7::storage