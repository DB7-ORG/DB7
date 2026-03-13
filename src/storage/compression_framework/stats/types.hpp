#pragma once

enum struct StatsType
{
    Number,
    String
};

struct IStats
{
    u32 num_items;
    StatsType type;
    u8 size_of_type;
};