#include "access/index/varlen_layout/btree_varlen_models.hpp"

namespace db7::access
{
    VarlenHeader *CastHeader(byte *data)
    {
        return reinterpret_cast<VarlenHeader *>(data);
    }

    void WriteHeader(VarlenHeader *header, u64 rlink, u32 count, u8 level, u64 max_val)
    {
        header->rlink = rlink;
        header->count = count;
        header->level = level;
        header->max_val = max_val;
    }

    void WriteHeader(byte *data, u64 rlink, u32 count, u8 level, u64 max_val)
    {
        auto *header = CastHeader(data);
        WriteHeader(header, rlink, count, level, max_val);
    }
}