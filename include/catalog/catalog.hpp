#pragma once

#include "catalog_common.hpp"

namespace db7::catalog
{
    /**
     * This component manages all metadata including tables, databases, schemas, constraints and many more.
     * Its implemented using relational model and all operations inside it should work just like any other table operation.
     * Meaning its fully transactional and recoverable in case of errors
     */
    class Catalog
    {
    private:
        //
    public:
        database_oid_t CreateDatabase(std::string &name);
    };

}