#include "access/index/btree_header.hpp"

namespace db7::access
{
    BtreeHeader *CastHeader(byte *data)
    {
        return reinterpret_cast<BtreeHeader *>(data);
    }

    void WriteHeader(BtreeHeader *header, u64 rlink, u32 count, u8 level, u64 max_val)
    {
        header->rlink = rlink;
        header->count = count;
        header->level = level;
        header->max_val = max_val;
    }

    void IncrementHeaderSize(BtreeHeader *header)
    {
        header->count++;
    }
}