#include "debug/inspect.hpp"

#ifdef DB7_DEBUG_FLAG

#include "third_party/unordered_dense.h"
#define private public
#include "catalog/catalog.hpp"
#undef private

#include "access/index/btree.hpp"

#include <cstring>
#include <limits>

extern db7::catalog::Catalog *g_catalog;

namespace db7::debug
{
    extern "C" PageDump *InspectVarlenLayout(byte *data)
    {
        static PageDump dump{};
        dump = {};
        auto *header = reinterpret_cast<access::VarlenHeader<u64> *>(data);

        dump.pid = header->pid;
        dump.rlink = header->rlink;
        dump.llink = header->llink;
        dump.count = header->count;
        dump.level = header->level;
        dump.max_val = header->max_val;
        dump.heap_size = header->heap_size;
        dump.prefix_len = header->prefix_len;

        bool is_leaf = header->level == 0;

        // max_val key
        u64 undefined = is_leaf
                            ? std::numeric_limits<u64>::max()
                            : std::numeric_limits<page_id>::max();

        if (header->max_val != undefined)
        {
            byte *ptr = data + header->max_val;
            u16 len = 0;
            char *key_data = nullptr;

            if (is_leaf)
            {
                auto *hdr = reinterpret_cast<access::SlotValHeader<u64> *>(ptr);
                len = hdr->len;
                key_data = reinterpret_cast<char *>(ptr + sizeof(access::SlotValHeader<u64>));
            }
            else
            {
                auto *hdr = reinterpret_cast<access::SlotValHeader<page_id> *>(ptr);
                len = hdr->len;
                key_data = reinterpret_cast<char *>(ptr + sizeof(access::SlotValHeader<page_id>));
            }
            u16 copy = std::min(len, (u16)255);
            memcpy(dump.max_val_key, key_data, copy);
            dump.max_val_key[copy] = '\0';
        }
        else
        {
            strcpy(dump.max_val_key, "(UNDEFINED)");
        }

        if (header->prefix_offset != std::numeric_limits<u32>::max())
        {
            byte *ptr = data + header->prefix_offset;

            u16 len = 0;
            char *key_data = nullptr;

            if (is_leaf)
            {
                len = header->prefix_len;
                key_data =
                    reinterpret_cast<char *>(ptr);
            }
            else
            {
                len = header->prefix_len;
                key_data =
                    reinterpret_cast<char *>(ptr);
            }

            u16 copy = std::min(len, (u16)255);

            memcpy(dump.prefix_key, key_data, copy);
            dump.prefix_key[copy] = '\0';
        }
        else
        {
            strcpy(dump.prefix_key, "(NONE)");
        }

        // slots
        access::Slot *slots = reinterpret_cast<access::Slot *>(data + sizeof(access::VarlenHeader<u64>));
        u32 count = std::min(header->count, (u32)1024);

        for (u32 i = 0; i < count; i++)
        {
            byte *ptr = data + slots[i].offset;
            u16 len = 0;
            u64 res = 0;
            char *key_data = nullptr;

            if (is_leaf)
            {
                auto *hdr = reinterpret_cast<access::SlotValHeader<u64> *>(ptr);
                len = hdr->len;
                res = hdr->result;
                key_data = reinterpret_cast<char *>(ptr + sizeof(access::SlotValHeader<u64>));
            }
            else
            {
                auto *hdr = reinterpret_cast<access::SlotValHeader<page_id> *>(ptr);
                len = hdr->len;
                res = hdr->result;
                key_data = reinterpret_cast<char *>(ptr + sizeof(access::SlotValHeader<page_id>));
            }

            dump.slots[i].slot = i;
            dump.slots[i].offset = slots[i].offset;
            dump.slots[i].len = len;
            dump.slots[i].result_val = res;
            u16 copy = std::min(len, (u16)255);
            memcpy(dump.slots[i].key, key_data, copy);
            dump.slots[i].key[copy] = '\0';
        }

        return &dump;
    }

    using OID = catalog::CatalogTableOid;

    static const std::unordered_map<u32, access::Table * catalog::DatabaseCatalog::*> tbl_map = {
        {static_cast<u32>(OID::PG_NAMESPACE), &catalog::DatabaseCatalog::namespaces_},
        {static_cast<u32>(OID::PG_CLASS), &catalog::DatabaseCatalog::classes_},
        {static_cast<u32>(OID::PG_ATTRIBUTE), &catalog::DatabaseCatalog::attributes_},
        {static_cast<u32>(OID::PG_TYPE), &catalog::DatabaseCatalog::types_},
        {static_cast<u32>(OID::PG_CONSTRAINT), &catalog::DatabaseCatalog::constraints_},
        {static_cast<u32>(OID::PG_LANGUAGE), &catalog::DatabaseCatalog::languages_},
        {static_cast<u32>(OID::PG_PROC), &catalog::DatabaseCatalog::procs_},
    };
    static const std::unordered_map<u32, access::BTreeIndex<u64> * catalog::DatabaseCatalog::*> idx_map = {
        // pg_namespace indexes
        {static_cast<u32>(OID::PG_INDEX_NAMESPACE_NSPOID), &catalog::DatabaseCatalog::namespaces_index_nspoid_},
        {static_cast<u32>(OID::PG_INDEX_NAMESPACE_NSPNAME), &catalog::DatabaseCatalog::namespaces_index_nspname_},

        // pg_class indexes
        {static_cast<u32>(OID::PG_INDEX_CLASS_RELOID), &catalog::DatabaseCatalog::classes_index_reloid_},
        {static_cast<u32>(OID::PG_INDEX_CLASS_RELNAME), &catalog::DatabaseCatalog::classes_index_relname_},
        {static_cast<u32>(OID::PG_INDEX_CLASS_RELNAMESPACE), &catalog::DatabaseCatalog::classes_index_relnamespace_},

        // pg_attribute indexes
        {static_cast<u32>(OID::PG_INDEX_ATTRIBUTE_ATTNUM), &catalog::DatabaseCatalog::attributes_index_attnum_},
        {static_cast<u32>(OID::PG_INDEX_ATTRIBUTE_ATTRELID), &catalog::DatabaseCatalog::attributes_index_attrelid_},
        {static_cast<u32>(OID::PG_INDEX_ATTRIBUTE_ATTNAME), &catalog::DatabaseCatalog::attributes_index_attname_},

        // pg_type indexes
        {static_cast<u32>(OID::PG_INDEX_TYPE_TYPOID), &catalog::DatabaseCatalog::types_index_typoid_},
        {static_cast<u32>(OID::PG_INDEX_TYPE_TYPNAME), &catalog::DatabaseCatalog::types_index_typname_},
        {static_cast<u32>(OID::PG_INDEX_TYPE_TYPNAMESPACE), &catalog::DatabaseCatalog::types_index_typnamespace_},

        // pg_constraint indexes
        {static_cast<u32>(OID::PG_INDEX_CONSTRAINT_CONOID), &catalog::DatabaseCatalog::constraints_index_conoid_},
        {static_cast<u32>(OID::PG_INDEX_CONSTRAINT_CONNAME), &catalog::DatabaseCatalog::constraints_index_conname_},
        {static_cast<u32>(OID::PG_INDEX_CONSTRAINT_CONNAMESPACE), &catalog::DatabaseCatalog::constraints_index_connamespace_},
        {static_cast<u32>(OID::PG_INDEX_CONSTRAINT_CONRELID), &catalog::DatabaseCatalog::constraints_index_conrelid_},
        {static_cast<u32>(OID::PG_INDEX_CONSTRAINT_CONINDID), &catalog::DatabaseCatalog::constraints_index_conindid_},
        {static_cast<u32>(OID::PG_INDEX_CONSTRAINT_CONFRELID), &catalog::DatabaseCatalog::constraints_index_confrelid_},

        // pg_language indexes
        {static_cast<u32>(OID::PG_INDEX_LANGUAGE_LANOID), &catalog::DatabaseCatalog::languages_index_lanoid_},
        {static_cast<u32>(OID::PG_INDEX_LANGUAGE_LANNAME), &catalog::DatabaseCatalog::languages_index_lanname_},

        // pg_proc indexes
        {static_cast<u32>(OID::PG_INDEX_PROC_PROOID), &catalog::DatabaseCatalog::procs_index_prooid_},
        {static_cast<u32>(OID::PG_INDEX_PROC_PRONAME), &catalog::DatabaseCatalog::procs_index_proname_},
    };

    static access::BTreeIndex<u64> *FindIndex(u32 tbl_id)
    {
        if (!g_catalog)
            return nullptr;

        if (tbl_id == static_cast<u32>(OID::PG_INDEX_DATABASE_DATOID))
            return g_catalog->databases_index_datoid;
        if (tbl_id == static_cast<u32>(OID::PG_INDEX_DATABASE_DATNAME))
            return g_catalog->databases_index_datname;

        auto it2 = g_catalog->databases_map_.begin();
        if (it2 == g_catalog->databases_map_.end())
            return nullptr;
        auto dbc = it2->second;

        if (!dbc)
            return nullptr;

        auto it = idx_map.find(tbl_id);
        if (it != idx_map.end())
            return dbc->*(it->second);

        return nullptr;
    }

    static access::Table *FindTable(u32 tbl_id)
    {
        if (!g_catalog)
            return nullptr;

        if (tbl_id == static_cast<u32>(OID::PG_DATABASES))
            return g_catalog->databases_;

        auto it2 = g_catalog->databases_map_.begin();
        if (it2 == g_catalog->databases_map_.end())
            return nullptr;
        auto dbc = it2->second;

        if (!dbc)
            return nullptr;

        auto it = tbl_map.find(tbl_id);
        if (it != tbl_map.end())
            return dbc->*(it->second);

        return nullptr;
    }

    extern "C" LayoutType GetLayoutType(table_id tbl_id)
    {
        if (FindIndex(tbl_id) != nullptr)
        {
            return LayoutType::Index; // 0
        }
        if (FindTable(tbl_id) != nullptr)
        {
            return LayoutType::Table; // 1
        }
        return LayoutType::Unknown; // 2
    }

    extern "C" HeapDump *InspectHeapLayout(byte *data, table_id tbl_id)
    {
        static HeapDump dump{};
        dump = {};

        access::Table *tbl = FindTable(tbl_id); // todo will need to add this to index also
        if (!tbl)
            return &dump;

        auto *header = storage::PageHeader::CastHeader(data);
        dump.row_count = std::min(header->count, (u32)1024);

        dump.column_count = std::min((u32)tbl->schema_.GetCount(), (u32)64);

        for (u32 i = 0; i < dump.row_count; i++)
        {
            u32 byte_offset = storage::HEADER_SIZE + i / 8;
            dump.deleted[i] = (data[byte_offset] >> (i % 8)) & 1;

            u32 c = 0;
            for (auto column : tbl->schema_)
            {
                u32 type_size = column.GetTypeSize();
                const byte *val_ptr = tbl->layout_.Get(data, c, i);

                auto name = column.GetName();
                u32 name_copy = std::min((u32)name.size(), (u32)63);
                std::memcpy(dump.columns[i][c].name, name.data(), name_copy);
                dump.columns[i][c].name[name_copy] = '\0';

                if (type_size <= 8)
                {
                    i64 ival = 0;
                    std::memcpy(&ival, val_ptr, type_size);
                    snprintf(dump.columns[i][c].val, sizeof(dump.columns[i][c].val), "%ld", ival);
                }
                else
                {
                    auto entry = *reinterpret_cast<const storage::VarlenEntry *>(val_ptr);
                    u32 copy = std::min(entry.GetSize(), (u32)1023);
                    std::memcpy(dump.columns[i][c].val, entry.GetInline(), copy);
                    dump.columns[i][c].val[copy] = '\0';
                }
            }
        }

        return &dump;
    }
}

#endif