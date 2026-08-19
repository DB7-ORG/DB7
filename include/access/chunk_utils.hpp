#pragma once

#include "access/data_chunk.hpp"
#include "storage/page.hpp"
#include "access/schema.hpp"
#include "storage/layouts/pax.hpp"

namespace db7::access
{
    class ChunkUtils
    {
    public:
        static void ReadSingleIntoChunk(const Schema &schema, const storage::PaxLayout &layout, DataChunk *chunk, storage::Page *page, u32 idx)
        {
            byte *data = page->GetData();
            auto iter = chunk->InitIterator();
            for (auto id : chunk->GetColumnIds())
            {
                auto info = schema.GetColumn(id);
                byte *ptr = layout.Get(data, info.GetPosiiton(), idx);
                iter.PushBack(std::span<byte>(ptr, info.GetTypeSize()));
            }
        }

        static u32 InsertBulk(const Schema &schema, const storage::PaxLayout &layout, DataChunk *chunk, storage::Page *page, u32 item_count = 1)
        {
            byte *data = page->GetData();
            u32 old_count = layout.IncrementHeaderCount(data, item_count);

            for (auto id : chunk->GetColumnIds())
            {
                auto info = schema.GetColumn(id);
                layout.Insert(data, std::span<byte>(chunk->Access(info.GetPosiiton()), info.GetTypeSize() * item_count), info.GetPosiiton(), old_count);
            }

            return old_count;
        }

        static void UpdateSingle(const Schema &schema, const storage::PaxLayout &layout, DataChunk *chunk, DataChunk *delta, storage::Page *page, u32 idx)
        {
            auto delta_iter = delta->InitIterator();
            auto chunk_iter = chunk->InitIterator();
            for (auto id : chunk->GetColumnIds())
            {
                auto column_info = schema.GetColumn(id);
                byte *ptr = layout.Get(page->GetData(), column_info.GetPosiiton(), idx);
                delta_iter.PushBack({ptr, column_info.GetTypeSize()});
                memcpy(ptr, chunk_iter.Next(), column_info.GetTypeSize());
            }
        }

        static void Merge(const Schema &schema, DataChunk *curr_chunk, DataChunk *new_chunk)
        {
            int new_idx = 0;
            for (auto new_id : new_chunk->GetColumnIds())
            {
                int cur_idx = 0;
                for (auto cur_id : curr_chunk->GetColumnIds())
                {
                    if (cur_id == new_id)
                    {
                        u32 size = schema.GetColumn(new_id).GetTypeSize();
                        memcpy(curr_chunk->Access(cur_idx), new_chunk->Access(new_idx), size);
                        break;
                    }
                    cur_idx++;
                }
                new_idx++;
            }
        }
    };
} // namespace db7::access
