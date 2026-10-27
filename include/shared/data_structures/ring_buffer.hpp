#pragma once

#include "common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/models/result_object.hpp"
#include <memory>

namespace db7::shared {

template <typename T>
class Ringbuffer {
private:
  u32 head_;
  u32 tail_;
  u32 mask_;
  std::unique_ptr<T[]> buffer_;

public:
  explicit Ringbuffer(u32 min_capacity) : head_(0), tail_(0) {
    auto cap = std::bit_ceil(std::max(min_capacity, 1u));
    mask_ = cap - 1;
    buffer_ = std::make_unique<T[]>(cap);
  }

  u32 Head() const { return head_; }
  u32 Tail() const { return tail_; }
  u32 Size() const { return head_ - tail_; }
  bool Empty() const { return tail_ == head_; }
  u32 MaxSize() const { return mask_ + 1; }
  bool Full() const { return Size() == MaxSize(); }

  void Append(T data) {
    DB7_ASSERT(!Full(), "Ring is already full");
    auto cur = (head_++) & mask_;
    buffer_[cur] = data;
  }

  void Append(const T *data, u32 count) {
    DB7_ASSERT(Size() + count <= MaxSize(), "Ring overflow");
    for (u32 i = 0; i < count; i++) buffer_[(head_ + i) & mask_] = data[i];
    head_ += count;
  }

  T ReadUnsafe() {
    DB7_ASSERT(!Empty(), "Ring is already full");
    auto cur = (tail_++) & mask_;
    return buffer_[cur];
  }

  ResultObj<T> Read() {
    if (Empty()) { return ResultObj<T>::Fail(); }
    return ReadUnsafe();
  }
};
} // namespace db7::shared