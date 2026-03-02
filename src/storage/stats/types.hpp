#pragma once

enum struct StatsType
{
    Number,
    String
};

struct IStats
{
    StatsType type;
    u8 size_of_type;
};