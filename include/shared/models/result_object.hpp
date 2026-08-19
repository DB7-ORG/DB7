#pragma once

#include "common.hpp"

#include <concepts>

namespace db7
{
    template <typename Typ>
    struct ResultObj
    {
        const char *message;

        Typ value;

        bool success;

        static_assert(!std::is_same_v<Typ, char *>, "Typ cant be char*");

        ResultObj() : success(true) {};

        ResultObj(const char *message, bool success) : message(message), success(success) {}

        ResultObj(Typ value, bool success = true) : value(value), success(success) {}

        static ResultObj Fail(const char *m = nullptr) { return {m, false}; }
    };

    template <>
    struct ResultObj<void>
    {
        const char *message = nullptr;
        bool success = false;

        static ResultObj Ok() { return {nullptr, true}; }
        static ResultObj Fail(const char *m = nullptr) { return {m, false}; }
    };
}