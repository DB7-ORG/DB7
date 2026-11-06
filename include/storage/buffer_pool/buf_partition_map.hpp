#pragma once

#include "shared/hash_util.hpp"
#include "shared/locks/adaptive_version_lock.hpp"
#include "storage/page.hpp"
#include "third_party/unordered_dense.h"

namespace db7::storage {

template <typename Key, typename Val, int PCount>
class BufPartitionedMap {
  static_assert(PCount > 0 && (PCount & (PCount - 1)) == 0, "PCount must be a power of 2");

private:
  struct alignas(64) AlignedLock {
    shared::AdaptiveVersionLock lock;
  };
  using Map = ankerl::unordered_dense::map<Key, Val>;
  Map partition_maps_[PCount];
  AlignedLock partition_locks_[PCount];

  Val GetUnsafe(Map &partition_map, Key id) {
    auto it = partition_map.find(id);
    if (it != partition_map.end()) { return it->second; }
    return NotFound();
  }

public:
  ~BufPartitionedMap() {}

  constexpr Val NotFound() const {
    if constexpr (std::is_pointer_v<Val>)
      return nullptr;
    else if constexpr (std::is_integral_v<Val>)
      return std::numeric_limits<Val>::max();
    else
      DB7_UNIMPLEMENTED();
  }

  bool IsValid(Val value) const { return value != NotFound(); }

  u32 GetPartitionIdx(Key id) { return HashUtil::murmurhash64(id.packed) % PCount; }

  void Reserve(size_t n, u32 partIdx) {
    shared::AdaptiveVersionLock::WriteGuard guard(partition_locks_[partIdx].lock);
    partition_maps_[partIdx].reserve(n);
  }

  template <typename Fn, typename FnRes>
  Val GetAndApply(Key id, u32 partIdx, Fn &&on_found, FnRes &result) {
    auto &partition = partition_maps_[partIdx];
    shared::AdaptiveVersionLock::ReadGuard guard(partition_locks_[partIdx].lock);
    Val val = GetUnsafe(partition, id);
    if (IsValid(val)) result = on_found(val);
    return val;
  }

  template <typename OnInserted, typename OnExists>
  bool PutAndApply(Key id, Val val, Val &new_val, u32 partIdx, OnInserted &&on_found,
                   OnExists &&on_exists, flags_t &state) {
    auto &partition = partition_maps_[partIdx];
    shared::AdaptiveVersionLock::WriteGuard guard(partition_locks_[partIdx].lock);
    auto [it, inserted] = partition.try_emplace(id, val);
    new_val = it->second;
    if (inserted) {
      on_found(it->second);
    } else {
      state = on_exists(it->second);
    }
    return inserted;
  }

  /** NOTE: used when u know entry cant exit in the table */
  template <typename OnInserted>
  void PutAndApply(Key id, Val val, u32 partIdx, OnInserted &&on_found) {
    auto &partition = partition_maps_[partIdx];
    shared::AdaptiveVersionLock::WriteGuard guard(partition_locks_[partIdx].lock);
    auto [it, _] = partition.emplace()(id, val);
    on_found(it->second);
  }

  void Delete(Key id, Val old_val) {
    auto part = GetPartitionIdx(id);
    return Delete(id, old_val, part);
  }

  void Delete(Key id, Val old_val, u32 partIdx) {
    auto &partition = partition_maps_[partIdx];
    shared::AdaptiveVersionLock::WriteGuard guard(partition_locks_[partIdx].lock);
    auto it = partition.find(id);
    if (it != partition.end() && it->second == old_val) { partition.erase(it); }
  }
};
} // namespace db7::storage