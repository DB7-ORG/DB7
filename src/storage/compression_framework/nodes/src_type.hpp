#pragma once

#include "common.hpp"

enum struct SrcType
{
    U8,
    U16,
    U32,
    U64,
    DBL
};

template <typename Func>
inline void DispatchType(SrcType type, Func &&f)
{
    switch (type)
    {
    case SrcType::U8:
        f.template operator()<u8>();
        break;
    case SrcType::U16:
        f.template operator()<u16>();
        break;
    case SrcType::U32:
        f.template operator()<u32>();
        break;
    case SrcType::U64:
        f.template operator()<u64>();
        break;
    case SrcType::DBL:
        f.template operator()<double>();
        break;
    default:
        throw std::runtime_error("unsupported type");
    }
}

inline u32 TypeSize(SrcType t)
{
    switch (t)
    {
    case SrcType::U8:
        return sizeof(u8);
    case SrcType::U16:
        return sizeof(u16);
    case SrcType::U32:
        return sizeof(u32);
    case SrcType::U64:
        return sizeof(u64);
    case SrcType::DBL:
        return sizeof(double);
    }
    std::terminate();
}

inline u32 SizeOfBuffer(SrcType type, u32 nitems)
{
    return TypeSize(type) * nitems;
}