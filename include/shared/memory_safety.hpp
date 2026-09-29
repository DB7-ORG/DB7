#pragma once

namespace db7 {

template <bool IS_ENABLED>
struct MemorySafety { // TODO check this flas works
#ifdef DEBUG
  // In DEBUG mode safety is always on
  static constexpr bool ENABLED = true;
#else
  static constexpr bool ENABLED = IS_ENABLED;
#endif
};

} // namespace db7