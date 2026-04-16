#pragma once

#include "catalog_common.hpp"

namespace db7::catalog
{
    /// forward refs
    class DatabaseCatalog;

    class Builder
    {
    public:
        static DatabaseCatalog *CreateDatabaseCatalog();
    };
}