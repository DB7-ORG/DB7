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
    private:
        catalog::db_oid_t db_id_;

    public:
        // cached data
        access::Table *namespaces_;
        access::Table *classes_;
        access::Table *attributes_;
        access::Table *types_;
        access::Table *constraints_;
        access::Table *languages_;
        access::Table *procs_;

        DatabaseCatalog(catalog::db_oid_t db_id) : db_id_(db_id) {}

        ~DatabaseCatalog()
        {
            delete namespaces_;
            delete classes_;
            delete attributes_;
            delete types_;
            delete constraints_;
            delete languages_;
            delete procs_;
        }

        catalog::db_oid_t GetDbOid()
        {
            return db_id_;
        }
    };
}