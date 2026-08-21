#pragma once

#include "common.hpp"
#include "shared/models/result_object.hpp"

#include <concepts>

namespace db7::access
{
    struct BaseLyHeader
    {
        page_id pid;
        page_id rlink;
        u16 count;
        u16 max_val;
        u8 level;

        static u8 GetLevel(byte *data)
        {
            return reinterpret_cast<BaseLyHeader *>(data)->level;
        }

        static u32 GetCount(byte *data)
        {
            return reinterpret_cast<BaseLyHeader *>(data)->count;
        }
    };

    struct Key
    {
        u16 len;
        byte *data;

        Key() : len(0), data(nullptr) {}

        Key(u16 len, byte *data)
            : len(len), data(data) {}
    };

    struct TypeSize
    {
        catalog::col_oid_t col_id;
        type_id type;
        u16 size;

        TypeSize(catalog::col_oid_t id, type_id t) : col_id(id), type(t), size(SizeOf(t)) {}
    };

    inline std::vector<TypeSize> AttrsFor(catalog::CatalogTableOid t)
    {
        using enum catalog::CatalogTableOid;
        using enum catalog::CatalogColumnOid;
        switch (t)
        {
        // pg_database
        case PG_INDEX_DATABASE_DATOID:
            return {{DATOID, type_id::INTEGER}};
        case PG_INDEX_DATABASE_DATNAME:
            return {{DATNAME, type_id::VARCHAR}};

        // pg_namespace
        case PG_INDEX_NAMESPACE_NSPOID:
            return {{NSPOID, type_id::INTEGER}};
        case PG_INDEX_NAMESPACE_NSPNAME:
            return {{NSPNAME, type_id::VARCHAR}};

        // pg_class
        case PG_INDEX_CLASS_RELOID:
            return {{RELOID, type_id::INTEGER}};
        case PG_INDEX_CLASS_RELNAME:
            return {{RELNAME, type_id::VARCHAR}};
        case PG_INDEX_CLASS_RELNAMESPACE:
            return {{RELNAMESPACE, type_id::INTEGER}};

        // pg_attribute
        case PG_INDEX_ATTRIBUTE_ATTNUM:
            return {{ATTNUM, type_id::SMALLINT}};
        case PG_INDEX_ATTRIBUTE_ATTRELID_ATTNAME:
            return {{ATTRELID, type_id::INTEGER}, {ATTNAME, type_id::VARCHAR}};

        // pg_type
        case PG_INDEX_TYPE_TYPOID:
            return {{TYPOID, type_id::INTEGER}};
        case PG_INDEX_TYPE_TYPNAME:
            return {{TYPNAME, type_id::VARCHAR}};
        case PG_INDEX_TYPE_TYPNAMESPACE:
            return {{TYPNAMESPACE, type_id::INTEGER}};

        // pg_constraint
        case PG_INDEX_CONSTRAINT_CONOID:
            return {{CONOID, type_id::INTEGER}};
        case PG_INDEX_CONSTRAINT_CONNAME:
            return {{CONNAME, type_id::VARCHAR}};
        case PG_INDEX_CONSTRAINT_CONNAMESPACE:
            return {{CONNAMESPACE, type_id::INTEGER}};
        case PG_INDEX_CONSTRAINT_CONRELID:
            return {{CONRELID, type_id::INTEGER}};
        case PG_INDEX_CONSTRAINT_CONINDID:
            return {{CONINDID, type_id::INTEGER}};
        case PG_INDEX_CONSTRAINT_CONFRELID:
            return {{CONFRELID, type_id::INTEGER}};

        // pg_language
        case PG_INDEX_LANGUAGE_LANOID:
            return {{LANOID, type_id::INTEGER}};
        case PG_INDEX_LANGUAGE_LANNAME:
            return {{LANNAME, type_id::VARCHAR}};

        // pg_proc
        case PG_INDEX_PROC_PROOID:
            return {{PROOID, type_id::INTEGER}};
        case PG_INDEX_PROC_PRONAME:
            return {{PRONAME, type_id::VARCHAR}};
        case PG_INDEX_INDEX_INDOID:
            return {{INDOID, type_id::INTEGER}};
        case PG_INDEX_INDEX_INDRELID:
            return {{INDRELID, type_id::INTEGER}};
        default:
            return {};
        }
        return {};
    }
}