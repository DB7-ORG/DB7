#pragma once

#include "common.hpp"

#include <vector>

namespace db7::shared {
class BitSet {
  std::vector<u64> words_;

public:
  void Add(column_t c) {
    const size_t w = c / 64;
    if (w >= words_.size()) { words_.resize(w + 1, 0); }
    words_[w] |= u64{1} << (c % 64);
  }
  bool Contains(column_t c) const {
    const size_t w = c / 64;
    return w < words_.size() && ((words_[w] >> (c % 64)) & 1);
  }
  bool Intersects(const BitSet &other) const {
    const size_t n = std::min(words_.size(), other.words_.size());
    for (size_t i = 0; i < n; i++) {
      if (words_[i] & other.words_[i]) { return true; }
    }
    return false;
  }
};
} // namespace db7::shared