#pragma once

#include "access/access_common.hpp"
#include "catalog/catalog_common.hpp"
#include "access/schema.hpp"
#include "storage/varlen_entry.hpp"

#include <vector>
#include <span>
#include <cstring>

namespace db7::access
{
    class DataChunk;

    constexpr u32 SIZE_PART = 2 * sizeof(u32);
    constexpr u32 COLUMN_IDS_START = shared::AlignUp(SIZE_PART, u32(sizeof(catalog::col_oid_t)));

    class DataChunkLayout
    {
    private:
        u32 column_count_;
        u32 total_size_;

        u32 header_size_;
        byte *header_underlying_;

        catalog::col_oid_t *GetColumnIdsPtr() { return reinterpret_cast<catalog::col_oid_t *>(header_underlying_ + COLUMN_IDS_START); }

    public:
        DB7_DISALLOW_COPY(DataChunkLayout);

        DataChunkLayout(const Schema &schema);

        DataChunkLayout(
            std::span<const catalog::col_oid_t> col_ids,
            std::span<const u16> attr_sizes);

        DataChunkLayout(
            std::initializer_list<catalog::col_oid_t> col_ids,
            std::initializer_list<u16> attr_sizes)
            : DataChunkLayout(
                  std::span<const catalog::col_oid_t>(col_ids.begin(), col_ids.size()),
                  std::span<const u16>(attr_sizes.begin(), attr_sizes.size())) {}

        ~DataChunkLayout() { delete[] header_underlying_; }

        u32 GetTotalSize() { return total_size_; }

        std::span<catalog::col_oid_t> GetColumnIds() { return std::span<catalog::col_oid_t>(GetColumnIdsPtr(), column_count_); };

        DataChunk *CreateDataChunk();

        DataChunk *CreateDataChunk(void *dest);
    };

    class DataChunk
    {
    private:
        u32 column_count_;
        u32 total_size_;
        u64 varlen_contents_[0];

        byte *GetUnderlyingPtr() { return reinterpret_cast<byte *>(this); }

        catalog::col_oid_t *GetColumnIdsPtr() { return reinterpret_cast<catalog::col_oid_t *>(GetUnderlyingPtr() + COLUMN_IDS_START); }

        u16 *GetOffsetsPtr()
        {
            auto aligned = shared::AlignUp(uintptr_t(GetColumnIdsPtr() + column_count_), uintptr_t(sizeof(u32)));
            return reinterpret_cast<u16 *>(aligned);
        }

        byte *GetDataPtr() { return reinterpret_cast<byte *>(GetOffsetsPtr() + column_count_); }

    public:
        DB7_DISALLOW_COPY(DataChunk);

        std::span<byte> GetUnderlying() { return std::span<byte>(GetUnderlyingPtr(), total_size_); }

        std::span<byte> GetHeaderPtr() { return std::span<byte>(GetUnderlyingPtr(), GetHeaderSize()); }

        std::span<catalog::col_oid_t> GetColumnIds() { return std::span<catalog::col_oid_t>(GetColumnIdsPtr(), column_count_); }

        byte *Access(u32 idx) { return GetUnderlyingPtr() + GetOffsetsPtr()[idx]; }

        byte *Get(catalog::col_oid_t oid)
        {
            int idx = 0;
            for (auto id : GetColumnIds())
            {
                if (id == oid)
                {
                    return Access(idx);
                }
                idx++;
            }

            throw std::runtime_error("Tried to access invalid column");
        }

        template <typename T>
        void Write(catalog::col_oid_t oid, T new_data)
        {
            int idx = 0;
            for (auto id : GetColumnIds())
            {
                if (id == oid)
                {
                    *(T *)(Access(idx)) = new_data;
                    // std::memcpy(, &new_data, sizeof(new_data));
                    break;
                }
                idx++;
            }
        }

        void Write(catalog::col_oid_t oid, std::span<byte> new_data)
        {
            int idx = 0;
            for (auto id : GetColumnIds())
            {
                if (id == oid)
                {
                    std::memcpy(Access(idx), new_data.data(), new_data.size());
                    break;
                }
                idx++;
            }
        }

        u32 GetSize() { return total_size_; }

        u32 GetHeaderSize() { return GetDataPtr() - GetUnderlyingPtr(); }

        u32 GetColumnCount() { return column_count_; }

        class Iterator
        {
        private:
            u32 iterator_idx_;

            byte *iterator_data_ptr_;

            u16 *iterator_offsets_;

        public:
            Iterator(DataChunk *chunk)
            {
                iterator_idx_ = 0;
                iterator_data_ptr_ = chunk->GetUnderlyingPtr();
                iterator_offsets_ = chunk->GetOffsetsPtr();
            }

            void PushBack(std::span<byte> new_data)
            {
                auto off = iterator_offsets_[iterator_idx_++];
                std::memcpy(iterator_data_ptr_ + off, new_data.data(), new_data.size());
            }

            template <typename T>
            void PushBack(T new_data)
            {
                DB7_ASSERT(shared::IsAligned<T>(iterator_data_ptr_ + iterator_offsets_[iterator_idx_]), "unaligned write");
                *(T *)(iterator_data_ptr_ + iterator_offsets_[iterator_idx_++]) = new_data;
            }

            byte *Next()
            {
                return iterator_data_ptr_ + iterator_offsets_[iterator_idx_++];
            }
        };

        Iterator InitIterator()
        {
            return Iterator(this);
        }

        void Print(Schema *schema);
    };

    class DataChunkBuilder
    {
    public:
        static void BuildDatabaseChunk(DataChunk *chunk, catalog::db_oid_t oid, const std::span<byte> name)
        {
            auto iter = chunk->InitIterator();
            iter.PushBack(oid);
            storage::VarlenEntry entry;
            entry.Set(name);
            iter.PushBack(entry);
        }

        static void BuildNamespaceChunk(DataChunk *chunk, catalog::namespace_oid_t oid, const std::span<byte> name)
        {
            chunk->Write(catalog::CatalogColumnOid::NSPOID, oid);
            storage::VarlenEntry entry;
            entry.Set(name);
            chunk->Write(catalog::CatalogColumnOid::NSPNAME, entry);
        }

        static void BuildNamespaceChunk(DataChunk *chunk, catalog::namespace_oid_t oid)
        {
            chunk->Write(catalog::CatalogColumnOid::NSPOID, oid);
        }

        static void BuildClassChunk(
            DataChunk *chunk,
            catalog::class_oid_t oid)
        {
            chunk->Write(catalog::CatalogColumnOid::RELOID, oid);
        }

        static void BuildClassChunk(
            DataChunk *chunk,
            catalog::class_oid_t oid,
            const std::span<byte> name,
            catalog::namespace_oid_t namespace_oid,
            char kind,
            const std::span<byte> options)
        {
            chunk->Write(catalog::CatalogColumnOid::RELOID, oid);
            storage::VarlenEntry entry;
            entry.Set(name);
            chunk->Write(catalog::CatalogColumnOid::RELNAME, entry);
            chunk->Write(catalog::CatalogColumnOid::RELNAMESPACE, namespace_oid);
            chunk->Write(catalog::CatalogColumnOid::RELKIND, kind);
            storage::VarlenEntry options_entry;
            options_entry.Set(options);
            chunk->Write(catalog::CatalogColumnOid::RELOPTIONS, options_entry);
        }

        static void BuildAttributeChunk(
            DataChunk *chunk,
            catalog::attribute_oid_t oid,
            catalog::class_oid_t rel_oid,
            const std::span<char> name,
            access::type_id type_oid,
            u16 attr_len,
            bool not_null)
        {
            chunk->Write(catalog::CatalogColumnOid::ATTNUM, oid);
            chunk->Write(catalog::CatalogColumnOid::ATTRELID, rel_oid);
            storage::VarlenEntry entry;
            entry.Set(name);
            chunk->Write(catalog::CatalogColumnOid::ATTNAME, entry);
            chunk->Write(catalog::CatalogColumnOid::ATTTYPID, type_oid);
            chunk->Write(catalog::CatalogColumnOid::ATTLEN, attr_len);
            chunk->Write(catalog::CatalogColumnOid::ATTNOTNULL, not_null);
        }

        static void BuildIndexChunk(
            DataChunk *chunk,
            catalog::index_oid_t oid,
            catalog::class_oid_t rel_oid,
            bool is_unique,
            bool is_primary,
            bool is_exclusion,
            bool is_imediate,
            bool is_valid,
            bool is_ready,
            bool is_live,
            u8 index_type)
        {
            chunk->Write(catalog::CatalogColumnOid::INDOID, oid);
            chunk->Write(catalog::CatalogColumnOid::INDRELID, rel_oid);
            chunk->Write(catalog::CatalogColumnOid::INDISUNIQUE, is_unique);
            chunk->Write(catalog::CatalogColumnOid::INDISPRIMARY, is_primary);
            chunk->Write(catalog::CatalogColumnOid::INDISEXCLUSION, is_exclusion);
            chunk->Write(catalog::CatalogColumnOid::INDIMMEDIATE, is_imediate);
            chunk->Write(catalog::CatalogColumnOid::INDISVALID, is_valid);
            chunk->Write(catalog::CatalogColumnOid::INDISREADY, is_ready);
            chunk->Write(catalog::CatalogColumnOid::INDISLIVE, is_live);
            chunk->Write(catalog::CatalogColumnOid::IND_TYPE, index_type);
        }
    };
}