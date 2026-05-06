#pragma once

#include "storage/buffer_pool/buffer_pool.hpp"
#include "access/schema.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "access/projected_rows.hpp"
#include "storage/storage_common.hpp"
#include "storage/disk_manager/disk_manager_async.hpp"
#include "storage/varlen_entry.hpp"

#include <unordered_map>
#include <memory>
#include <span>

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
        catalog::rel_oid_t varlen_oid_;

    public:
        DB7_DISALLOW_COPY(Table);

        Table(storage::BufferPool *buffer, storage::DiskManagerAsync *disk_mng, Schema schema, catalog::rel_oid_t oid, catalog::rel_oid_t varlen_oid = 0)
            : buffer_(buffer), disk_mng_(disk_mng), schema_(std::move(schema)), oid_(oid), varlen_oid_(varlen_oid)
        {
            // TODO initialize a table file using disk manager
            if (!disk_mng_->CreateOpenFile(oid_, 1))
            {
                // TODO handle error
                DB7_ASSERT(false, "Table could not be created/opened");
            }

            if (varlen_oid_ != INVALID_REL_OID && !disk_mng_->CreateOpenFile(varlen_oid_, 1))
            {
                // TODO handle error
                DB7_ASSERT(false, "Table could not be created/opened");
            }
        }

        void Insert(const ProjectedRows &rows);

        std::pair<u32, u32> Insert(std::span<const byte> data);

        void Delete(u32 idx, catalog::rel_oid_t pid);

        u32 PageCount();

        Schema *GetSchema()
        {
            return &schema_;
        }

        void PrintPage(storage::Page *page);
    };
}