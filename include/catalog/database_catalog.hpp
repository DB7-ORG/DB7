#pragma once

#include "access/table.hpp"

#include <vector>

namespace db7::catalog
{
    /**
     * Database catalog is a component managed by db7::catalog::Catalog.
     * Catalog isnt managing this component because the database lifetime is strongly tied to DatabaseCatalog lifetime.
     * Meaning this object exists as long as the database.
     * This component stores cached catalog entries. They can be evicetd only using dll modifications when cache is invalidated.
     */
    class DatabaseCatalog
    {
    public:
        // cached data
        access::Table *namespaces_;
        access::Table *classes_;
        access::Table *columns_;

        DatabaseCatalog() {}
    };
}