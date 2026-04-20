#pragma once

#include "storage/buffer_pool/buffer_pool.hpp"
#include "access/schema.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "access/projected_rows.hpp"

#include <unordered_map>
#include <memory>

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
        Schema schema_;
        catalog::rel_oid_t oid_;

    public:
        DB7_DISALLOW_COPY(Table);

        Table(storage::BufferPool *buffer, Schema schema, catalog::rel_oid_t oid)
            : buffer_(buffer), schema_(std::move(schema)), oid_(oid)
        {
            // TODO initialize a table file using disk manager
        }

        void Insert(const ProjectedRows &rows);

        u32 PageCount() { return 0; };

        void Scan() {};

        Schema *GetSchema()
        {
            return &schema_;
        }
    };
}