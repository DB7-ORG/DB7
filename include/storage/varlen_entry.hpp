#pragma once

#include "storage/storage_common.hpp"

#include <algorithm>
#include <cstring>
#include <span>
#include <string_view>

namespace db7::storage {
struct VarlenRef {
  page_id pid;
  u32 offset;
};

constexpr size_t INLINE_SIZE_CAP = sizeof(VarlenRef) + sizeof(u32);

struct VarlenEntry {
private:
  u32 size_;
  u32 prefix_;
  union {
    VarlenRef ref_content_;
    char inline_content_[sizeof(VarlenRef)];
  };

public:
  VarlenEntry() {}

  u32 GetSize() const { return size_; }
  u32 GetPrefix() const { return prefix_; }
  VarlenRef GetRef() { return ref_content_; }
  const char *GetInline() const { return reinterpret_cast<const char *>(&prefix_); }
  bool IsInline() const { return size_ <= INLINE_SIZE_CAP; }

  void Set(const std::span<byte> data) {
    size_ = data.size();
    std::memcpy(&prefix_, data.data(), std::min(data.size(), INLINE_SIZE_CAP));
  }

  void Set(const std::span<const byte> data) {
    size_ = data.size();
    std::memcpy(&prefix_, data.data(), std::min(data.size(), INLINE_SIZE_CAP));
  }

  void Set(const std::span<const char> data) {
    size_ = data.size();
    std::memcpy(&prefix_, data.data(), std::min(data.size(), INLINE_SIZE_CAP));
  }

  void Set(const std::span<byte> data, page_id pid, u32 offset) {
    size_ = data.size();
    std::memcpy(&prefix_, data.data(),
                data.size() < sizeof(prefix_) ? data.size() : sizeof(prefix_));
    ref_content_ = {pid, offset};
  }

  static VarlenEntry Create(byte *content, u32 size, bool reclaim = false) {
    VarlenEntry result;
    result.Set(std::span<byte>{content, size});
    return result;
  }

  static VarlenEntry Create(const byte *content, u32 size, bool reclaim = false) {
    VarlenEntry result;
    result.Set(std::span<const byte>{content, size});
    return result;
  }

  std::string_view StringView() const { return std::string_view(GetInline(), GetSize()); }

  static constexpr u32 InlineThreshold() { return sizeof(VarlenEntry) - sizeof(u32); }

  int Compare(const std::span<byte> data) const {
    const char *inlined = GetInline();
    u32 cmp_len = std::min((u32)data.size(), GetSize());
    int result = std::memcmp(inlined, data.data(), cmp_len);
    if (result != 0) return result;

    if (GetSize() < data.size())
      return -1;
    else if (GetSize() > data.size())
      return 1;
    else
      return 0;
  }
};

static_assert(sizeof(VarlenEntry) == 16);
static_assert(INLINE_SIZE_CAP == 12);
} // namespace db7::storage