#pragma once

#include "common.hpp"
#include "hash_util.hpp"

//===----------------------------------------------------------------------===//
//
// Fixed point decimals
//
//===----------------------------------------------------------------------===//
namespace noisepage::execution::sql
{
    /**
     * A generic fixed point decimal value. This only serves as a storage container for decimals of various sizes.
     * Operations on decimals require a precision and scale.
     *
     * @tparam T The underlying native data type sufficiently large to store decimals of a pre-determined scale.
     */
    template <typename T>
    class Decimal
    {
    private:
        // The encoded decimal value
        T value_;

    public:
        /** Underlying native data type. */
        using NativeType = T;

        /**
         * Empty constructor
         */
        Decimal() = default;

        /**
         * Create a decimal value using the given raw underlying encoded value.
         * @param value The value to set this decimal to.
         */
        explicit Decimal(const T &value) : value_(value) {}

        /**
         * @return The raw underlying encoded decimal value.
         */
        operator T() const { return value_; } // NOLINT

        /**
         * Compute the hash value of this decimal instance.
         * @param seed The value to seed the hash with.
         * @return The hash value for this decimal instance.
         */
        hash_t Hash(const hash_t seed) const { return common::HashUtil::HashCrc(value_); }

        /**
         * @return The hash value of this decimal instance.
         */
        hash_t Hash() const { return Hash(0); }

        /**
         * Add the encoded decimal value @em that to this decimal value.
         * @param that The value to add.
         * @return This decimal value.
         */
        const Decimal<T> &operator+=(const T &that)
        {
            value_ += that;
            return *this;
        }

        /**
         * Subtract the encoded decimal value @em that from this decimal value.
         * @param that The value to subtract.
         * @return This decimal value.
         */
        const Decimal<T> &operator-=(const T &that)
        {
            value_ -= that;
            return *this;
        }

        /**
         * Multiply the encoded decimal value @em that with this decimal value.
         * @param that The value to multiply by.
         * @return This decimal value.
         */
        const Decimal<T> &operator*=(const T &that)
        {
            value_ *= that;
            return *this;
        }

        /**
         * Divide this decimal value by the encoded decimal value @em that.
         * @param that The value to divide by.
         * @return This decimal value.
         */
        const Decimal<T> &operator/=(const T &that)
        {
            value_ /= that;
            return *this;
        }

        /**
         * Modulo divide this decimal value by the encoded decimal value @em that.
         * @param that The value to modulus by.
         * @return This decimal value.
         */
        const Decimal<T> &operator%=(const T &that)
        {
            value_ %= that;
            return *this;
        }
    };

    using Decimal32 = Decimal<i32>;
    using Decimal64 = Decimal<i64>;
    using Decimal128 = Decimal<i128>;

    class Timestamp;

    static constexpr i64 K_MICRO_SECONDS_PER_SECOND = 1000 * 1000;
    static constexpr i64 K_MICRO_SECONDS_PER_MINUTE = 60UL * K_MICRO_SECONDS_PER_SECOND;
    static constexpr i64 K_MICRO_SECONDS_PER_HOUR = 60UL * K_MICRO_SECONDS_PER_MINUTE;
    static constexpr i64 K_MICRO_SECONDS_PER_DAY = 24UL * K_MICRO_SECONDS_PER_HOUR;

    class Date
    {
    public:
        /**
         * The internal representation of a SQL date.
         */
        using NativeType = i32;

        /**
         * Empty constructor.
         */
        Date() = default;

        /**
         * @return True if this is a valid date instance; false otherwise.
         */
        bool IsValid() const;

        /**
         * @return A string representation of this date in the form "YYYY-MM-MM".
         */
        std::string ToString() const;

        /**
         * @return The year of this date.
         */
        i32 ExtractYear() const;

        /**
         * @return The month of this date.
         */
        i32 ExtractMonth() const;

        /**
         * @return The day of this date.
         */
        i32 ExtractDay() const;

        /**
         * Convert this date object into its year, month, and day parts.
         * @param[out] year The year corresponding to this date.
         * @param[out] month The month corresponding to this date.
         * @param[out] day The day corresponding to this date.
         */
        void ExtractComponents(i32 *year, i32 *month, i32 *day);

        /**
         * Convert this date instance into a timestamp instance.
         * @return The timestamp instance representing this date.
         */
        Timestamp ConvertToTimestamp() const;

        /**
         * Compute the hash value of this date instance.
         * @param seed The value to seed the hash with.
         * @return The hash value for this date instance.
         */
        hash_t Hash(const hash_t seed) const { return common::HashUtil::HashCrc(value_, seed); }

        /**
         * @return The hash value of this date instance.
         */
        hash_t Hash() const { return Hash(0); }

        /**
         * @return True if this date equals @em that date; false otherwise.
         */
        bool operator==(const Date &that) const { return value_ == that.value_; }

        /**
         * @return True if this date is not equal to @em that date; false otherwise.
         */
        bool operator!=(const Date &that) const { return value_ != that.value_; }

        /**
         * @return True if this data occurs before @em that date; false otherwise.
         */
        bool operator<(const Date &that) const { return value_ < that.value_; }

        /**
         * @return True if this data occurs before or is the same as @em that date; false otherwise.
         */
        bool operator<=(const Date &that) const { return value_ <= that.value_; }

        /**
         * @return True if this date occurs after @em that date; false otherwise.
         */
        bool operator>(const Date &that) const { return value_ > that.value_; }

        /**
         * @return True if this date occurs after or is equal to @em that date; false otherwise.
         */
        bool operator>=(const Date &that) const { return value_ >= that.value_; }

        /** @return The native representation of the date (julian microseconds). */
        Date::NativeType ToNative() const;

        /**
         * Construct a Date with the specified native representation.
         * @param val The native representation of a date.
         * @return The constructed Date.
         */
        static Date FromNative(Date::NativeType val);

        /**
         * Convert a C-style string of the form "YYYY-MM-DD" into a date instance. Will attempt to convert
         * the first date-like object it sees, skipping any leading whitespace.
         * @param str The string to convert.
         * @param len The length of the string.
         * @return The constructed date. May be invalid.
         */
        static Date FromString(const char *str, std::size_t len);

        /**
         * Convert a string of the form "YYYY-MM-DD" into a date instance. Will attempt to convert the
         * first date-like object it sees, skipping any leading whitespace.
         * @param str The string to convert.
         * @return The constructed Date. May be invalid.
         */
        static Date FromString(std::string_view str) { return FromString(str.data(), str.size()); }

        /**
         * Create a Date instance from a specified year, month, and day.
         * @param year The year of the date.
         * @param month The month of the date.
         * @param day The day of the date.
         * @return The constructed date. May be invalid.
         */
        static Date FromYMD(i32 year, i32 month, i32 day);

        /**
         * Is the date corresponding to the given year, month, and day a valid date?
         * @param year The year of the date.
         * @param month The month of the date.
         * @param day The day of the date.
         * @return True if valid date.
         */
        static bool IsValidDate(i32 year, i32 month, i32 day);

    private:
        friend class Timestamp;
        friend struct DateVal;
        friend class PostgresPacketWriter;

        // Private constructor to force static factories.
        explicit Date(NativeType value) : value_(value) {}

    private:
        // Date value
        NativeType value_;
    };

    class Timestamp
    {
    public:
        /** The internal representation of a SQL timestamp. */
        using NativeType = u64;

        /**
         * Empty constructor.
         */
        Timestamp() = default;

        /**
         * @return The year of this timestamp.
         */
        i32 ExtractYear() const;

        /**
         * @return The month of this timestamp.
         */
        i32 ExtractMonth() const;

        /**
         * @return The day of this timestamp.
         */
        i32 ExtractDay() const;

        /**
         * @return The year of this timestamp.
         */
        i32 ExtractHour() const;

        /**
         * @return The month of this timestamp.
         */
        i32 ExtractMinute() const;

        /**
         * @return The day of this timestamp.
         */
        i32 ExtractSecond() const;

        /**
         * @return The milliseconds of this timestamp.
         */
        i32 ExtractMillis() const;

        /**
         * @return The microseconds of this timestamp.
         */
        i32 ExtractMicros() const;

        /**
         * @return The day-of-the-week (0-6 Sun-Sat) this timestamp falls on.
         */
        i32 ExtractDayOfWeek() const;

        /**
         * @return THe day-of-the-year this timestamp falls on.
         */
        i32 ExtractDayOfYear() const;

        /**
         * Extract all components of this timestamp
         * @param[out] year The year corresponding to this date.
         * @param[out] month The month corresponding to this date.
         * @param[out] day The day corresponding to this date.
         * @param[out] hour The hour corresponding to this date.
         * @param[out] min The minute corresponding to this date.
         * @param[out] sec The second corresponding to this date.
         * @param[out] millisec The millisecond corresponding to this date.
         * @param[out] microsec The millisecond corresponding to this date.
         */
        void ExtractComponents(i32 *year, i32 *month, i32 *day, i32 *hour, i32 *min, i32 *sec,
                               i32 *millisec, i32 *microsec) const;

        /**
         * Convert this timestamp instance into a date instance.
         * @return The date instance representing this timestamp.
         */
        Date ConvertToDate() const;

        /**
         * Compute the hash value of this timestamp instance.
         * @param seed The value to seed the hash with.
         * @return The hash value for this timestamp instance.
         */
        hash_t Hash(const hash_t seed) const { return common::HashUtil::HashCrc(value_, seed); }

        /**
         * @return The hash value of this timestamp instance.
         */
        hash_t Hash() const { return Hash(0); }

        /**
         * @return True if this timestamp equals @em that timestamp; false otherwise.
         */
        bool operator==(const Timestamp &that) const { return value_ == that.value_; }

        /**
         * @return True if this timestamp is not equal to @em that timestamp; false otherwise.
         */
        bool operator!=(const Timestamp &that) const { return value_ != that.value_; }

        /**
         * @return True if this data occurs before @em that timestamp; false otherwise.
         */
        bool operator<(const Timestamp &that) const { return value_ < that.value_; }

        /**
         * @return True if this data occurs before or is the same as @em that timestamp; false otherwise.
         */
        bool operator<=(const Timestamp &that) const { return value_ <= that.value_; }

        /**
         * @return True if this timestamp occurs after @em that timestamp; false otherwise.
         */
        bool operator>(const Timestamp &that) const { return value_ > that.value_; }

        /**
         * @return True if this timestamp occurs after or is equal to @em that timestamp; false otherwise.
         */
        bool operator>=(const Timestamp &that) const { return value_ >= that.value_; }

        /** @return The native representation of the timestamp. */
        Timestamp::NativeType ToNative() const;

        /**
         * Construct a Timestamp with the specified native representation.
         * @param val The native representation of a timestamp.
         * @return The constructed timestamp.
         */
        static Timestamp FromNative(Timestamp::NativeType val);

        /**
         * @return A string representation of timestamp in the form "YYYY-MM-DD HH:MM:SS.ZZZ"
         */
        std::string ToString() const;

        /**
         * Convert a C-style string of the form "YYYY-MM-DD HH::MM::SS" into a timestamp. Will attempt to
         * convert the first timestamp-like object it sees, skipping any leading whitespace.
         * @param str The string to convert.
         * @param len The length of the string.
         * @return The constructed Timestamp. May be invalid.
         */
        static Timestamp FromString(const char *str, std::size_t len);

        /**
         * Convert a string of the form "YYYY-MM-DD HH::MM::SS" into a timestamp. Will attempt to convert
         * the first timestamp-like object it sees, skipping any leading whitespace.
         * @param str The string to convert.
         * @return The constructed Timestamp. May be invalid.
         */
        static Timestamp FromString(std::string_view str) { return FromString(str.data(), str.size()); }

        /**
         * Instantiate a timestamp with the specified number of microseconds in Julian time.
         * @param usec The number of microseconds in Julian time.
         * @return The constructed timestamp.
         */
        static Timestamp FromMicroseconds(u64 usec) { return Timestamp(usec); }

        /**
         * Given time components parse the timezone and construct an adjusted TPL timestamp. If any
         * component is invalid, the bool result value will be false.
         * @param c The starting character, which is a '+' or '-'
         * @param year The year.
         * @param month The month.
         * @param day The day.
         * @param hour The hour.
         * @param min The minute.
         * @param sec The second.
         * @param milli The millisecond.
         * @param micro The microsecond.
         * @param ptr Start of timestamp string.
         * @param limit End of timestamp string.
         * @return The constructed timestamp if valid.
         */
        static Timestamp AdjustTimeZone(char c, i32 year, i32 month, i32 day, i32 hour, i32 min,
                                        i32 sec, i32 milli, i32 micro, const char *ptr, const char *limit);

        /**
         * Given year, month, day, hour, minute, second components construct a TPL timestamp. If any
         * component is invalid, the bool result value will be false.
         * @param year The year.
         * @param month The month.
         * @param day The day.
         * @param hour The hour.
         * @param min The minute.
         * @param sec The second.
         * @return The constructed timestamp if valid.
         */
        static Timestamp FromYMDHMS(i32 year, i32 month, i32 day, i32 hour, i32 min, i32 sec);

        /**
         * Given year, month, day, hour, minute, second, ms, and us components construct a TPL timestamp. If any
         * component is invalid, the bool result value will be false.
         * @param year The year.
         * @param month The month.
         * @param day The day.
         * @param hour The hour.
         * @param min The minute.
         * @param sec The second.
         * @param milli The millisecond.
         * @param micro The microsecond.
         * @return The constructed timestamp if valid.
         */
        static Timestamp FromYMDHMSMU(i32 year, i32 month, i32 day, i32 hour, i32 min, i32 sec,
                                      i32 milli, i32 micro);

    private:
        friend class Date;
        friend struct TimestampVal;

        explicit Timestamp(NativeType value) : value_(value) {}

    private:
        // Timestamp value -- the native type denotes the microseconds with respect to Julian time
        NativeType value_;
    };
}