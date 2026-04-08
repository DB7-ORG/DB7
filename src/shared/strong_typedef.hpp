#pragma once

#include "json.hpp"
#include <cstdint>
#include <ostream>
#include <type_traits>
#include <utility>

namespace noisepage::common
{
    /*
     * A strong typedef is like a typedef, except the compiler will enforce explicit
     * conversion for you.
     *
     * Usually, typedefs (or equivalent 'using' statement) are transparent to the
     * compiler. If you declare A and B to both be int, they are interchangeable.
     * This is not exactly ideal because then it becomes easy for you to do something
     * like this:
     *
     * // some definition
     * A foo(A a, B b);
     *
     * // invocation
     * (a = 42, b = 10)
     * foo(10, 42); // oops
     *
     * ... and the compiler will happily compile and run that code with no warning.
     *
     * With a strong typedef, you are required to explicitly convert these types,
     * turning our example into:
     *
     * A a(42);
     * B b(10);
     * foo(a, b);
     *
     * Now foo(b, a) would be a type mismatch.
     *
     * To extract the primitive integral type that backs the strong typedef out
     * of an instance, use the StrongTypeAlias::UnderlyingValue() member. For example:
     *
     * using A = StrongTypeAlias<struct SomeTag, int>
     *
     * A a(42)
     * assert(a.UnderlyingValue() == 42)
     *
     * This mechanism works with all integral types (as defined by std::is_integral).
     *
     * In order to use this macro, you need to use STRONG_TYPEDEF_HEADER in the .h file, then
     * include common/strong_typedef_body.h in the corresponding .cpp file and use
     * STRONG_TYPEDEF_BODY with the same arguements. Finally, you need to add an explicit instantation
     * of the template in common/strong_typedef.cpp.
     *
     */
#define STRONG_TYPEDEF_HEADER(name, underlying_type)                                              \
    namespace tags                                                                                \
    {                                                                                             \
        struct name##_typedef_tag                                                                 \
        {                                                                                         \
        };                                                                                        \
    }                                                                                             \
    using name = ::noisepage::common::StrongTypeAlias<tags::name##_typedef_tag, underlying_type>; \
    namespace tags                                                                                \
    {                                                                                             \
        void to_json(nlohmann::json &j, const name &c);   /* NOLINT */                            \
        void from_json(const nlohmann::json &j, name &c); /* NOLINT */                            \
    }

    /**
     * A StrongTypeAlias is the underlying implementation of STRONG_TYPEDEF.
     *
     * Unless you know what you are doing, you shouldn't touch this class. Just use
     * the MACRO defined above
     * @tparam Tag a dummy class type to annotate the underlying type
     * @tparam IntType the underlying type
     */
    template <class Tag, typename IntType>
    class StrongTypeAlias
    {
        // TODO: perhaps remove ability to static_cast to underlying value altogether.
        static_assert(std::is_integral<IntType>::value, "Only int types are defined for strong typedefs");

    private:
        IntType val_;

    public:
        StrongTypeAlias() = default;

        /**
         * Constructs a new StrongTypeAlias.
         * @param val const reference to the underlying type.
         */
        constexpr explicit StrongTypeAlias(const IntType &val) : val_(val) {}
        /**
         * Move constructs a new StrongTypeAlias.
         * @param val const reference to the underlying type.
         */
        constexpr explicit StrongTypeAlias(IntType &&val) : val_(std::move(val)) {}

        /**
         * @return the underlying value
         */
        constexpr IntType &UnderlyingValue() { return val_; }

        /**
         * @return the underlying value
         */
        constexpr const IntType &UnderlyingValue() const { return val_; }

        /**
         *
         * @return the underlying value
         */
        explicit operator IntType() const { return val_; }

        /**
         * Checks if this is equal to the other StrongTypeAlias.
         * @param rhs the other StrongTypeAlias to be compared.
         * @return true if the StrongTypeAliases are equal, false otherwise.
         */
        bool operator==(const StrongTypeAlias &rhs) const { return val_ == rhs.val_; }

        /**
         * Checks if this is not equal to the other StrongTypeAlias.
         * @param rhs the other StrongTypeAlias to be compared.
         * @return true if the StrongTypeAliases are not equal, false otherwise.
         */
        bool operator!=(const StrongTypeAlias &rhs) const { return val_ != rhs.val_; }

        /**
         * prefix-increment.
         * @return the value of the variable after the modification.
         */
        StrongTypeAlias &operator++()
        {
            ++val_;
            return *this;
        }

        /**
         * postfix-increment.
         * @return the value of the variable before the modification.
         */
        StrongTypeAlias operator++(int) { return StrongTypeAlias(val_++); }

        /**
         * addition.
         * @param operand another int type
         * @return sum of the underlying value and given operand
         */
        StrongTypeAlias operator+(const IntType &operand) const
        {
            return StrongTypeAlias(static_cast<IntType>(val_ + operand));
        }

        /**
         * addition and assignment
         * @param rhs another int type
         * @return self-reference after the rhs is added to the underlying value
         */
        StrongTypeAlias &operator+=(const IntType &rhs)
        {
            val_ += rhs;
            return *this;
        }

        /**
         * prefix-decrement.
         * @return the value of the variable after the modification.
         */
        StrongTypeAlias &operator--()
        {
            --val_;
            return *this;
        }

        /**
         * postfix-decrement.
         * @return the value of the variable before the modification.
         */
        StrongTypeAlias operator--(int) { return StrongTypeAlias(val_--); }

        /**
         * subtraction
         * @param operand another int type
         * @return difference between the underlying value and given operand
         */
        StrongTypeAlias operator-(const IntType &operand) const { return StrongTypeAlias(val_ - operand); }

        /**
         * subtraction and assignment
         * @param rhs another int type
         * @return self-reference after the rhs is subtracted from the underlying value
         */
        StrongTypeAlias &operator-=(const IntType &rhs)
        {
            val_ -= rhs;
            return *this;
        }

        /**
         * @param other the other type alias to compare to
         * @return whether underlying value of this < other
         */
        bool operator<(const StrongTypeAlias &other) const { return val_ < other.val_; }

        /**
         * @param other the other type alias to compare to
         * @return whether underlying value of this < other
         */
        bool operator<=(const StrongTypeAlias &other) const { return val_ <= other.val_; }

        /**
         * @param other the other type alias to compare to
         * @return whether underlying value of this < other
         */
        bool operator>(const StrongTypeAlias &other) const { return val_ > other.val_; }

        /**
         * @param other the other type alias to compare to
         * @return whether underlying value of this < other
         */
        bool operator>=(const StrongTypeAlias &other) const { return val_ >= other.val_; }

        /**
         * Outputs the StrongTypeAlias to the output stream.
         * @param os output stream to be written to.
         * @param alias StrongTypeAlias to be output.
         * @return modified output stream.
         */
        friend std::ostream &operator<<(std::ostream &os, const StrongTypeAlias &alias) { return os << alias.val_; }

        /**
         * @return underlying value serialized to json
         */
        nlohmann::json ToJson() const;

        /**
         * @param j json to deserialize
         */
        void FromJson(const nlohmann::json &j);
    };
}