#pragma once

#include "catalog_common.hpp"
#include "access/schema.hpp"

namespace db7::storage
{
    class BufferPool; // forward declare, no #include needed
}

namespace db7::catalog
{
    /// forward refs
    class DatabaseCatalog;

    class Builder
    {
    public:
        static DatabaseCatalog *CreateDatabaseCatalog(storage::BufferPool *buffer_pool);

        static access::Schema CreateDatabaseSchema();
    };
}