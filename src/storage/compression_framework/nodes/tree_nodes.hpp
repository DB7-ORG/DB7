#pragma once

#include "common.hpp"
#include "../visitors/models.hpp"
#include "align_utils.hpp"
#include "types.hpp"
#include "slab_arena.hpp"
#include <cstring>

struct IntegerNode
{
    SlabArena *arena;
    UncompressedNode *unc;
    DictionaryNode *dict;
    RleNode *rle;
    BitpackNode *bp;
    u8 depth;
    u8 best_node_idx;

    IntegerNode(SlabArena *arena, u8 depth = 0) : arena(arena), depth(depth), best_node_idx(0)
    {
        unc = arena->New<UncompressedNode>(depth);
        dict = arena->New<DictionaryNode>(arena, depth);
        rle = arena->New<RleNode>(arena, depth);
        bp = arena->New<BitpackNode>(depth);
    }

    u32 Visit()
    {
        }
};
