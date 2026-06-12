#pragma once

#include "storage/undo_buffer.hpp"

#include <atomic>

namespace db7::storage
{
    struct VersionPtr // move this outside
    {
    private:
        std::atomic<UndoRecord *> ptr;

    public:
        UndoRecord *Get()
        {
            return ptr.load();
        }

        void Set(UndoRecord *new_ptr)
        {
            ptr.store(new_ptr);
        }
    };
}