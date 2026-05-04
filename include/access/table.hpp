#pragma once

#include "storage/buffer_pool/buffer_pool.hpp"
#include "access/schema.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "access/projected_rows.hpp"
#include "storage/storage_common.hpp"
#include "storage/disk_manager/disk_manager_async.hpp"

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
        storage::DiskManagerAsync *disk_mng_;
        Schema schema_;
        catalog::rel_oid_t oid_;

    public:
        DB7_DISALLOW_COPY(Table);

        Table(storage::BufferPool *buffer, storage::DiskManagerAsync *disk_mng, Schema schema, catalog::rel_oid_t oid)
            : buffer_(buffer), disk_mng_(disk_mng), schema_(std::move(schema)), oid_(oid)
        {
            // TODO initialize a table file using disk manager
            if (!disk_mng_->CreateOpenFile(oid, 1))
            {
                // TODO handle error
                DB7_ASSERT(false, "Table could not be created/opened");
            }
        }

        void Insert(const ProjectedRows &rows);

        u32 PageCount();

        void Scan() {};

        Schema *GetSchema()
        {
            return &schema_;
        }
    };
}