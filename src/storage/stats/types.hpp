#pragma once

enum struct StatsType
{
    Number,
    String
};

struct IStats
{
    StatsType type;
};