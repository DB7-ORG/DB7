#pragma once

#include "common.hpp"
#include <stddef.h>
#include <limits>

template <typename T>
constexpr T AlignUpPow2(T x, size_t align)
{
    return (x + align - 1) & ~(align - 1);
}

template <class T, size_t alignment>
T *MoveToBoundary(T *inbyte)
{
    return reinterpret_cast<T *>(
        (reinterpret_cast<uintptr_t>(inbyte) + (alignment - 1)) &
        ~(alignment - 1));
}

// use this when calling STL object if you want
// their memory to be aligned on cache lines
template <class T, size_t alignment>
class AlignedSTLAllocator
{
public:
    // type definitions
    typedef T value_type;
    typedef T *pointer;
    typedef const T *const_pointer;
    typedef T &reference;
    typedef const T &const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    // rebind allocator to type U
    template <class U>
    struct rebind
    {
        typedef AlignedSTLAllocator<U, alignment> other;
    };

    pointer address(reference value) const { return &value; }
    const_pointer address(const_reference value) const { return &value; }

    /* constructors and destructor
     * - nothing to do because the allocator has no state
     */
    AlignedSTLAllocator() {}
    AlignedSTLAllocator(const AlignedSTLAllocator &) {}
    template <typename U>
    AlignedSTLAllocator(const AlignedSTLAllocator<U, alignment> &) {}
    ~AlignedSTLAllocator() {}

    // return maximum number of elements that can be allocated
    size_type max_size() const throw()
    {
        return (std::numeric_limits<std::size_t>::max)() / sizeof(T);
    }

    /*
     *  allocate but don't initialize num elements of type T
     *
     *  This implementation is potentially unsafe on some compilers.
     */
    pointer allocate(size_type num, const void * = 0)
    {
        /**
         * The nasty trick here is to make the position of the actual pointer
         * within the newly allocated memory. The alternative is to use
         * some kind of data structure like a hash table, which might be slow.
         */
        size_t *buffer = reinterpret_cast<size_t *>(
            ::operator new(sizeof(uintptr_t) + (num + alignment) * sizeof(T)));
        size_t *answer = MoveToBoundary<size_t, alignment>(buffer + 1);
        *(answer - 1) = reinterpret_cast<uintptr_t>(answer) -
                        reinterpret_cast<uintptr_t>(buffer);
        return reinterpret_cast<pointer>(answer);
    }

    void construct(pointer p, const T &value)
    {
        // initialize memory with placement new
        new (p) T(value);
    }

    // destroy elements of initialized storage p
    void destroy(pointer p) { p->~T(); }

    // deallocate storage p of deleted elements
    void deallocate(pointer p, size_type /*num*/)
    {
        const size_t *assize_t = reinterpret_cast<size_t *>(p);
        const size_t offset = assize_t[-1];
        assert(offset > 0 && offset <= alignment + sizeof(size_t));
        ::operator delete(
            reinterpret_cast<pointer>(reinterpret_cast<uintptr_t>(p) - offset));
    }
};

// for our purposes, we don't want to distinguish between allocators.
template <class T1, size_t t, class T2>
bool operator==(const AlignedSTLAllocator<T1, t> &, const T2 &) throw()
{
    return true;
}

template <class T1, size_t t, class T2>
bool operator!=(const AlignedSTLAllocator<T1, t> &, const T2 &) throw()
{
    return false;
}
// typical cache line
typedef AlignedSTLAllocator<uint32_t, 64> cacheallocator;

namespace noisepage::common
{
    /**
     * Static utility class for more advanced memory allocation behavior
     */
    struct AllocationUtil
    {
        AllocationUtil() = delete;

        /**
         * Allocates a chunk of memory whose start address is guaranteed to be aligned to 8 bytes
         * @param byte_size size of the memory chunk to allocate, in bytes
         * @return allocated memory pointer
         */
        static byte *AllocateAligned(u64 byte_size)
        {
            // This is basically allocating the chunk as a 64-bit array, which forces c++ to give back to us
            // 8 byte-aligned addresses. + 7 / 8 is equivalent to padding up the nearest 8-byte size. We
            // use this hack instead of std::aligned_alloc because calling delete on it does not make ASAN
            // happy on Linux + GCC, and calling std::free on pointers obtained from new is undefined behavior.
            // Having to support two paradigms when we liberally use byte * throughout the codebase is a
            // maintainability nightmare.
            return reinterpret_cast<byte *>(new u64[(byte_size + 7) / 8]);
        }

        /**
         * Allocates an array of elements that start at an 8-byte aligned address
         * @tparam T type of element
         * @param size number of elements to allocate
         * @return allocated memory pointer
         */
        template <class T>
        static T *AllocateAligned(uint32_t size)
        {
            return reinterpret_cast<T *>(AllocateAligned(size * sizeof(T)));
        }
    };
}