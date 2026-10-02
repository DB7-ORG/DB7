#pragma once

#include <functional>

namespace db7 {

template <class T, class U>
bool RefersToSameObject(const T &a, const U &b) {
  static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U> || std::is_base_of_v<U, T>,
                "RefersToSameObject requires T and U to be related by inheritance");
  return static_cast<const void *>(&a) == static_cast<const void *>(&b);
}

//! Returns whether or not two reference wrappers refer to the same object
template <class T, class U>
bool RefersToSameObject(const std::reference_wrapper<T> &a, const std::reference_wrapper<U> &b) {
  return RefersToSameObject(a.get(), b.get());
}

} // namespace db7