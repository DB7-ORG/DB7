#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_header.hpp"

#include <span>

namespace db7::access
{
    struct Key
    {
        u16 len;
        byte *data;
    };
    using T = Key;

    class BtreeVarlenLayoutLeaf
    {
    private:
    public:
        BtreeVarlenLayoutLeaf(u64 header_size)
        {
        }

        auto Get(byte *data, const u32 count, const T value)
        {
            return 0;
        }

        T GetKeyAt(byte *data, u32 idx)
        {
            return {};
        }

        void Insert(byte *data, u32 count, T key, page_id value)
        {
        }

        template <typename Typ>
        u32 CopyUpperHalf(Typ *from, Typ *to, u32 count)
        {
        }

        bool HasSpace(BtreeHeader *header)
        {
            return true;
        }

        void CreateRoot(byte *data, T key, page_id pid, page_id new_pid)
        {
        }

        void Split(byte *data, byte *right_data, u32 count, T key, page_id value, T &sentinel_out, u32 &new_header_count_out, u32 &right_header_count_out)
        {
        }
    };
}