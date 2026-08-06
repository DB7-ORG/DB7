#pragma once

#include "common.hpp"

#include <concepts>

namespace db7::access
{
    struct BaseLyHeader
    {
        page_id pid;
        page_id rlink;
        u16 count;
        u16 max_val;
        u8 level;

        static u8 GetLevel(byte *data)
        {
            return reinterpret_cast<BaseLyHeader *>(data)->level;
        }

        static u32 GetCount(byte *data)
        {
            return reinterpret_cast<BaseLyHeader *>(data)->count;
        }
    };

    template <typename Typ>
    struct ResultObj
    {
        const char *message;

        Typ value;

        bool success;

        static_assert(!std::is_same_v<Typ, char *>, "Typ cant be char*");

        ResultObj() : success(true) {};

        ResultObj(const char *message, bool success) : message(message), success(success) {}

        ResultObj(Typ value, bool success) : value(value), success(success) {}
    };

    template <>
    struct ResultObj<void>
    {
        const char *message = nullptr;
        bool success = false;

        static ResultObj Ok() { return {nullptr, true}; }
        static ResultObj Fail(const char *m = nullptr) { return {m, false}; }
    };

    struct Key
    {
        u16 len;
        byte *data;

        Key() : len(0), data(nullptr) {}

        Key(u16 len, byte *data)
            : len(len), data(data) {}
    };

    struct TypeSize
    {
        catalog::col_oid_t col_id;
        type_id type;
        u16 size;

        TypeSize(catalog::col_oid_t id, type_id t) : col_id(id), type(t), size(SizeOf(t)) {}
    };
}