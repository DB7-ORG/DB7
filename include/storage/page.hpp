#pragma once

#include "common.hpp"
#include "shared/locks/adaptive_version_lock.hpp"
#include "shared/macro_helper.hpp"
#include "shared/sync/waiter_list.hpp"
#include "storage/mvcc/version_ptr.hpp"
#include "storage_common.hpp"

#include <atomic>
#include <condition_variable>
#include <cstring>

namespace db7 {

using flags_t = u32;

namespace storage {
class Page {
private:
  /**
   * Packed page state, same layout as Postgres' BufferDesc.state:
   *
   *   31        22 21     18 17            0
   *  +------------+---------+---------------+
   *  |  flags(10) | usage(4)|  ref count(18)|
   *  +------------+---------+---------------+
   *
   * Everything lives in one atomic word, so a single load is a consistent snapshot of all three
   * fields and a single CAS can check and update them together.
   */
  static constexpr flags_t kRefCountBits = 18;
  static constexpr flags_t kUsageCountBits = 4;
  static constexpr flags_t kFlagBits = 10;
  static_assert(kRefCountBits + kUsageCountBits + kFlagBits == 32, "State must fill 32 bits");

  static constexpr flags_t kRefCountOne = 1u;
  static constexpr flags_t kRefCountMask = (1u << kRefCountBits) - 1;

  static constexpr flags_t kUsageCountShift = kRefCountBits;
  static constexpr flags_t kUsageCountOne = 1u << kUsageCountShift;
  static constexpr flags_t kUsageCountMask = ((1u << kUsageCountBits) - 1) << kUsageCountShift;

  static constexpr flags_t kFlagShift = kRefCountBits + kUsageCountBits;

  static constexpr flags_t kDirtyFlag = 1u << (kFlagShift + 0);
  static constexpr flags_t kIOInProgressFlag = 1u << (kFlagShift + 1);
  static constexpr flags_t kWriteInProgressFlag = 1u << (kFlagShift + 2);
  static constexpr flags_t kTagValidFlag = 1u << (kFlagShift + 3);
  static constexpr flags_t kIOErrorFlag = 1u << (kFlagShift + 4);

  static constexpr flags_t RefCountOf(flags_t state) { return state & kRefCountMask; }
  static constexpr flags_t UsageCountOf(flags_t state) {
    return (state & kUsageCountMask) >> kUsageCountShift;
  }

  // TODO fix this
  static constexpr bool IsEvictableState(flags_t state) {
    return RefCountOf(state) == 0 && (state & (kIOInProgressFlag | kWriteInProgressFlag)) == 0;
  }

  PageIdentifier id_;                  // 8 bytes
  std::atomic<VersionPtr *> versions_; // 8 bytes
  byte *data_;                         // 8 bytes
  std::atomic<flags_t> state_;         // 4 bytes

  shared::WaitList waiters_;
  alignas(CACHE_LINE_SIZE) mutable std::mutex latch_; // ~56 bytes typically
  mutable std::condition_variable io_cv_;
  alignas(CACHE_LINE_SIZE) mutable shared::AdaptiveVersionLock lock_;

public:
  Page() : id_(PageIdentifier{0}), versions_(nullptr), data_(nullptr), state_(0) {}

  VersionPtr *GetVersions() { return versions_.load(); }
  void SetVersions(VersionPtr *ptr) { versions_.store(ptr); }
  /**
   * Data
   */
  byte *GetData() { return data_; }
  void SetData(byte *data) { data_ = data; }

  /**
   * ONLY USED FOR TESTING SHOULD BE REMOVED
   *//////////////////////////////////////////////////
  byte *GetOffset(u32 offset) {
    DB7_ASSERT(data_ != nullptr, "Page data in null");
    return data_ + offset;
  }

  template <typename T>
  void WriteOffset(u32 offset, T value) {
    DB7_ASSERT(data_ != nullptr, "Page data in null");
    memcpy(data_ + offset, &value,
           sizeof(T)); // TODO Compiler should optimize this for const fixed
                       // types, but test it.
  }

  void WriteOffset(u32 offset, const byte *value, u32 size) {
    DB7_ASSERT(data_ != nullptr, "Page data in null");
    memcpy(data_ + offset, value, size);
  }
  /////////////////////////////////////////////

  /**
   * ID
   */
  PageIdentifier GetId() const { return id_; }
  page_id GetPageId() const { return id_.pid; }
  void SetId(PageIdentifier id) { id_ = id; }
  PageIdentifier SwapId(PageIdentifier id) {
    auto old = id_;
    id_ = id;
    return old;
  }

  flags_t State() const { return state_.load(); }
  void StoreState(flags_t state) { state_.store(state); }
  bool CasState(u32 &expected, u32 desired) {
    return state_.compare_exchange_weak(expected, desired);
  }

  /**
   * Pins. Pin() and Unpin() return the previous state.
   *
   * NOTE: The ref count shares a word with usage and flags, so an overflow or underflow carries
   * into those fields instead of only corrupting the count.
   */
  flags_t Pin() {
    flags_t prev = state_.fetch_add(kRefCountOne);
    DB7_ASSERT(RefCountOf(prev) < kRefCountMask, "Pin count overflow");
    return prev;
  }
  flags_t Unpin() {
    flags_t prev = state_.fetch_sub(kRefCountOne);
    DB7_ASSERT(RefCountOf(prev) > 0, "Pin count underflow");
    return prev;
  }

  static bool IsPinned(flags_t state) { return RefCountOf(state) > 0; }
  static flags_t PinCount(flags_t state) { return RefCountOf(state); }
  static flags_t Pin(flags_t state) { return state + kRefCountOne; }
  static flags_t Unpin(flags_t state) { return state - kRefCountOne; }
  static bool IsIoInProgress(flags_t state) { return state & kIOInProgressFlag; }
  static flags_t SetIOInProgress(flags_t state) { return state | kIOInProgressFlag; }
  static flags_t ClearIOInProgress(flags_t state) { return state & (~kIOInProgressFlag); }
  static bool IsWriteInProgress(flags_t state) { return state & kWriteInProgressFlag; }
  static flags_t SetWriteInProgress(flags_t state) { return state | kWriteInProgressFlag; }
  static flags_t ClearWriteInProgress(flags_t state) { return state & (~kWriteInProgressFlag); }
  static bool IsDirty(flags_t state) { return state & kDirtyFlag; }
  static flags_t SetDirty(flags_t state) { return state | kDirtyFlag; }
  static flags_t ClearDirty(flags_t state) { return state & (~kDirtyFlag); }
  static flags_t ClearTagValid(flags_t state) { return state & ~kTagValidFlag; }
  static bool IsTagValid(flags_t state) { return state & kTagValidFlag; }
  static flags_t SetTagValid(flags_t state) { return state | kTagValidFlag; }
  static u8 UsageCount(flags_t state) { return static_cast<u8>(UsageCountOf(state)); }
  static flags_t DecUsage(flags_t s) { return s - kUsageCountOne; }

  static bool IsReclaimable(flags_t state) {
    // NOTE: does not take into account usage count
    return RefCountOf(state) == 0 && !IsWriteInProgress(state) && !IsIoInProgress(state);
  }
  /**
   * Flags
   */
  bool IsPinned() const { return RefCountOf(state_.load()); }
  bool IsIOInProgress() const { return state_.load() & kIOInProgressFlag; }
  void ClearIOInProgress() { state_.fetch_and(~kIOInProgressFlag); }
  bool IsWriteInProgress() const { return state_.load() & kWriteInProgressFlag; }
  void ClearWriteInProgress() { state_.fetch_and(~kWriteInProgressFlag); }
  void SetDirty() { state_.fetch_or(kDirtyFlag); }
  void SetTagValid() { state_.fetch_or(kTagValidFlag); }
  void SetError() { state_.fetch_or(kIOErrorFlag); }

  /**
   * NOTE: If i ever change this to value above 1 i should add a rule to ring strategy so it never
   * raises usage above 1, since it can by repeated access to the page make it look extremely HOT
   */
  static constexpr u8 kMaxUsage = 1;
  static_assert(kMaxUsage < (1u << kUsageCountBits), "kMaxUsage must fit in the usage count bits");

  // Returns true if the count changed.
  bool IncUsageCount() {
    flags_t cur = state_.load();
    return IncUsageCount(cur);
  }

  bool IncUsageCount(flags_t state) {
    do {
      if (UsageCountOf(state) >= kMaxUsage) return false;
    } while (!state_.compare_exchange_weak(state, state + kUsageCountOne));
    return true;
  }

  bool DecUsageCount() {
    flags_t cur = state_.load();
    return CheckDecUsageCount(cur);
  }

  bool CheckDecUsageCount(flags_t state) {
    do {
      if (UsageCount(state) == 0) { return false; }
    } while (!CasState(state, DecUsage(state)));
    return true;
  }

  /**
   * Locks
   */
  // void WLock() { latch_.lock(); }
  // void WUnlock() { latch_.unlock(); }
  // bool TryWLock() { return latch_.try_lock(); }

  shared::AdaptiveVersionLock &DataLock() const { return lock_; }
  void RDataLock() { lock_.ReadLock(); }
  bool TryRDataLock() { return lock_.TryReadLock(); }
  void RDataUnlock() { lock_.ReadUnlock(); }
  void WDataLock() { lock_.WriteLock(); }
  void WDataUnlock() { lock_.WriteUnlock(); }
  bool ReadVersion(u64 &version) { return lock_.ReadOptimistic(version); }
  bool ValidateVersion(u64 version) { return lock_.Validate(version); }

  /**
   * Channel
   */
  void SignalError() {
    DB7_ASSERT(IsIOInProgress() || IsWriteInProgress(), "Io/write in progress not set");
    SetError();
    waiters_.Signal();
  }

  void SignalIO() {
    DB7_ASSERT(IsIOInProgress(), "Io in progress not set");
    ClearIOInProgress();
    waiters_.Signal();
  }

  void SignalWrite() {
    DB7_ASSERT(IsWriteInProgress(), "write in progress not set");
    ClearWriteInProgress();
    waiters_.Signal();
  }

  void NotifyIO() { // TODO lockfree
    waiters_.Signal();
  }

  bool WaitIO(shared::Waiter *obj) {
    if (!IsIOInProgress()) { return; }
    return waiters_.TryPark(obj);
  }

  void WaitWrite(shared::Waiter *obj) {
    if (!IsIOOrWriteInProgress()) return;
    return waiters_.TryPark(obj);
  }

private:
  bool IsIOOrWriteInProgress() const {
    return state_.load() & (kIOInProgressFlag | kWriteInProgressFlag);
  }
};

} // namespace storage
} // namespace db7