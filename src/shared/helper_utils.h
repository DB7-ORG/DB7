#pragma once

#include <memory>

template <typename T>
T min(T a, T b)
{
    return (a < b) ? a : b;
}

template <class ValueType>
inline std::unique_ptr<ValueType[]>
make_uniq_array_uninitialized(size_t n)
{
    // Allocate raw memory without calling constructors
    void *raw = ::operator new(n * sizeof(ValueType));
    return std::unique_ptr<ValueType[]>(static_cast<ValueType *>(raw));
}

template <class T, typename... ARGS>
std::shared_ptr<T> make_buffer(ARGS &&...args)
{
    return std::make_shared<T>(std::forward<ARGS>(args...));
}