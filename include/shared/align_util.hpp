#pragma once

#include "shared/macro_helper.hpp"

#include "common.hpp"
#include <cstring>
#include <memory>

namespace db7::shared {

constexpr uintptr_t AlignUp(uintptr_t addr, u32 alignment) {
  return (addr + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
}

inline byte *AlignUp(byte *value, u32 alignment) {
  return reinterpret_cast<byte *>(
      AlignUp(reinterpret_cast<uintptr_t>(value), alignment));
}

template <typename T> constexpr T AlignDown(T value, size_t alignment) {
  return value & ~(alignment - 1);
}

template <typename T> static bool IsAligned(void *ptr) {
  return reinterpret_cast<uintptr_t>(ptr) % alignof(T) == 0;
}

struct AlignedDeleter {
  void operator()(void *ptr) const noexcept { std::free(ptr); }
};

using AlignedPtr = std::unique_ptr<byte[], AlignedDeleter>;

inline AlignedPtr AllocAligned(u32 size, u32 alignment) {
  u32 alloc_size = AlignUp(size, alignment);
  void *ptr = std::aligned_alloc(alignment, alloc_size);
  DB7_ASSERT(ptr != nullptr, "aligned_alloc failed");
  std::memset(ptr, 0, alloc_size);
  return AlignedPtr(static_cast<byte *>(ptr));
}
} // namespace db7::shared