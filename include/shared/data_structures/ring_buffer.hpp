#pragma once

#include "common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/models/result_object.hpp"

namespace db7::shared {

template <typename T>
class Ringbuffer {
  static constexpr u32 RING_SIZE = 16;
  static_assert((RING_SIZE & (RING_SIZE - 1)) == 0, "RING_SIZE must be a power of two");
  static constexpr u32 MASK = RING_SIZE - 1;

private:
  u32 head_;
  u32 tail_;
  T buffer_[RING_SIZE];

public:
  Ringbuffer() : head_(0), tail_(0) {}

  u32 Head() const { return head_; }
  u32 Tail() const { return tail_; }
  u32 Size() const { return head_ - tail_; }
  bool Empty() const { return tail_ == head_; }
  bool Full() const { return Size() == RING_SIZE; }
  constexpr u32 MaxSize() const { return RING_SIZE; }

  bool Append(T data) {
    DB7_ASSERT(!Full(), "Ring is already full");
    auto cur = (head_++) & MASK;
    buffer_[cur] = data;
  }

  T ReadUnsafe() {
    DB7_ASSERT(!Empty(), "Ring is already full");
    auto cur = (tail_++) & MASK;
    return buffer_[cur];
  }

  ResultObj<T> Read() {
    if (Empty()) { return ResultObj<T>::Fail(); }
    return ReadUnsafe();
  }
};
} // namespace db7::shared