#pragma once

#include "common.hpp"
#include "access/index/header.hpp"

namespace db7::access
{
    inline Key MakeEncodedKey(u16 len, byte *data)
    {
        return Key{len, len, data};
    }

    struct VarlenHeader : public BaseLyHeader
    {
        u16 heap_offset;

        static VarlenHeader *CastHeader(byte *data)
        {
            return reinterpret_cast<VarlenHeader *>(data);
        }

        void WriteHeader(page_id pid, page_id rlink, u16 count, u16 max_val, u8 level, u16 heap_offset)
        {
            this->pid = pid;
            this->rlink = rlink;
            this->count = count;
            this->max_val = max_val;
            this->level = level;
            this->heap_offset = heap_offset;
        }

        static void WriteHeader(byte *data, page_id pid, page_id rlink, u16 count, u16 max_val, u8 level, u16 heap_offset)
        {
            auto *header = CastHeader(data);
            header->WriteHeader(pid, rlink, count, max_val, level, heap_offset);
        }
    };

    struct Slot
    {
        u16 offset;
    };
};