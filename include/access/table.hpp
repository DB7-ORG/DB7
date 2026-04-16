#pragma once

#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/page_layout.hpp"
#include "catalog/catalog_common.hpp"

#include <unordered_map>
/**
 * Layer between storage and other components.
 * Used to implement abstractions that hide whether its working with row store or column store.
 * Idea is for this layer when its working in catalog to act as a row store
 * and when its working in excecution to operate as column store
 */
namespace db7::access
{
    /**
     * Table abstraction
     */
    class Table
    {
    private:
        storage::BufferPool *buffer_;
        storage::PageLayout *layout_;
        std::unordered_map<catalog::col_oid_t, u32> column_map_;

    public:
        Table(storage::BufferPool *buffer, storage::PageLayout *layout)
            : buffer_(buffer), layout_(layout) {}
    };
}