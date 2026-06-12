#pragma once

#include "shared/locks/adaptive_version_lock.hpp"
#include "third_party/concurrentqueue_safe.hpp"
#include "shared/error/exception.hpp"
#include "shared/macro_helper.hpp"

namespace db7::shared
{
    template <typename ObjectType>
    class ObjectPool
    {
    private:
        moodycamel::ConcurrentQueue<ObjectType *> queue_;
        u32 size_limit_;
        u32 reuse_limit_;
        std::atomic<u32> curr_size_;

    public:
        /**
         * Initializes a new object pool with the supplied limit to the number of
         * objects reused.
         *
         * @param size_limit the maximum number of objects the object pool controls
         * @param reuse_limit the maximum number of reusable objects
         */
        ObjectPool(u32 size_limit, u32 reuse_limit)
            : size_limit_(size_limit), reuse_limit_(reuse_limit), curr_size_(0) {}

        ~ObjectPool()
        {
            ObjectType *obj = nullptr;
            while (queue_.try_dequeue(obj))
            {
                delete obj;
            }
        }

        u32 GetCurrentSize()
        {
            return curr_size_.load();
        }

        ObjectType *Get()
        {
            ObjectType *obj = nullptr;
            if (queue_.try_dequeue(obj))
            {
                return obj;
            }
            u32 size = curr_size_++;
            if (size >= size_limit_)
            {
                curr_size_--;
                throw MEMORY_EXCEPTION("No more objects in the pool");
            }
            obj = new ObjectType();
            return obj;
        }

        void Release(ObjectType *obj)
        {
            DB7_ASSERT(obj != nullptr, "Releasing null pointer");

            if (curr_size_.load() >= reuse_limit_)
            {
                curr_size_--;
                delete obj;
            }
            else
            {
                curr_size_++;
                queue_.enqueue(obj);
            }
        }
    };
}