#pragma once

namespace db7::shared
{
    template <typename ValTyp>
    struct VectorValues
    {
        std::vector<ValTyp> vec;
        bool proceed;

        VectorValues() = default;

        size_t Size() const { return vec.size(); }

        ValTyp &operator[](size_t i) { return vec[i]; }

        const ValTyp &operator[](size_t i) const { return vec[i]; }
    };
}