#pragma once

#include "common.hpp"
#include "access/index/header.hpp"

namespace db7::access
{
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

    struct BaseLayout
    {
    protected:
        static constexpr auto header_size_ = sizeof(VarlenHeader);
        static constexpr page_id UNDEFINED_PAGE = std::numeric_limits<page_id>::max();
        static constexpr u16 UNDEFINED_OFFSET = std::numeric_limits<u16>::max();

        u16 *CastSlots(byte *data)
        {
            return reinterpret_cast<u16 *>(data + header_size_);
        }

        void ShiftRightInsert(u16 *slots, int idx, u16 count, u16 heap_offset)
        {
            std::memmove(slots + idx + 1, slots + idx, (count - idx) * sizeof(u16));
            slots[idx] = heap_offset;
        }

        void ShiftLeftDelete(u16 *slots, int idx, u16 count)
        {
            DB7_ASSERT(count >= 1, "Nothing to delete");
            std::memmove(slots + idx, slots + idx + 1, (count - idx - 1) * sizeof(u16));
        }

    public:
        void InitHeader(byte *data, u32 count, u8 level, page_id pid)
        {
            VarlenHeader::WriteHeader(data, pid, UNDEFINED_PAGE, count, UNDEFINED_OFFSET, level, DB7_PAGE_SIZE);
        }

        page_id GetRLink(byte *data)
        {
            return VarlenHeader::CastHeader(data)->rlink;
        }
    };
};