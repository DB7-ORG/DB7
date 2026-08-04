
#pragma once

#include "common.hpp"
#include "shared/align_util.hpp"
#include "access/index/layouts/varlen/varlen_layout_models.hpp"

#include <limits>
#include <algorithm>
#include <vector>

namespace db7::access
{
    template <typename ValTyp>
    class BtreeVarlenLayoutIntermediate
    {
    private:
        u64 key_offset_;

    public:
        static constexpr page_id UNDEFINED_PAGE = std::numeric_limits<page_id>::max();
        static constexpr u32 UNDEFINED_OFFSET = std::numeric_limits<u16>::max();

        BtreeVarlenLayoutIntermediate() : key_offset_(sizeof(VarlenHeader)) {}

        ValTyp Get(byte *data, const u32 count, const Key key)
        {
            DB7_UNIMPLEMENTED();
        }

        void Insert(byte *data, Key key, ValTyp value)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.enc_len != 0, "invalid key");
            DB7_UNIMPLEMENTED();
        }

        bool HasSpace(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.enc_len != 0, "invalid key");
            DB7_UNIMPLEMENTED();
        }

        bool HasSplit(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.enc_len != 0, "invalid key");
            DB7_UNIMPLEMENTED();
        }

        /**
         * TODO might be better to use thread local buffer for this case
         * to avoid copying objects
         */
        Key Split(byte *__restrict left_data, byte *__restrict right_data, ValTyp new_pid, Key key, ValTyp value)
        {
            DB7_UNIMPLEMENTED();
        }

        void CreateRoot(byte *data, Key key, ValTyp pid, ValTyp new_pid)
        {
            DB7_UNIMPLEMENTED();
        }

        void InitHeader(byte *data, u32 count, u8 level, page_id pid)
        {
            VarlenHeader::WriteHeader(data, pid, UNDEFINED_PAGE, count, UNDEFINED_OFFSET, level, DB7_PAGE_SIZE);
        }

        u32 GetRLink(byte *data)
        {
            return VarlenHeader::CastHeader(data)->rlink;
        }
    };
}