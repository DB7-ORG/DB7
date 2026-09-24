#pragma once

// #include "aligned_allocator.hpp"
#include "common.hpp"
// #include "runtime_types.hpp"
// #include "sql.hpp"
#include "shared/hash_util.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>

namespace db7::parser {
/**
 * A generic base catch-all SQL value. Used to represent a NULL-able SQL value.
 */
struct Val {
  /** NULL indication flag. */
  bool is_null_;

  /**
   * Construct a value with the given NULL indication.
   * @param is_null Whether the SQL value is NULL.
   */
  explicit Val(bool is_null) noexcept : is_null_(is_null) {}
};

/**
 * A NULL-able SQL boolean value.
 */
struct BoolVal : public Val {
  /** The raw boolean value. */
  bool val_;

  /**
   * Construct a non-NULL boolean with the given value.
   * @param val The value of the boolean.
   */
  explicit BoolVal(bool val) noexcept : Val(false), val_(val) {}

  /**
   * Convert this SQL boolean into a primitive boolean. Thanks to SQL's
   * three-valued logic, we implement the following truth table:
   *
   *   Value | NULL? | Output
   * +-------+-------+--------+
   * | false | false | false  |
   * | false | true  | false  |
   * | true  | false | true   |
   * | true  | true  | false  |
   * +-------+-------+--------+
   *
   * @return The primitive boolean value corresponding to this SQL Boolean.
   */
  bool ForceTruth() const noexcept { return !is_null_ && val_; }

  /**
   * @return A NULL boolean value.
   */
  static BoolVal Null() {
    BoolVal val(false);
    val.is_null_ = true;
    return val;
  }
};

/**
 * A NULL-able integral SQL value. Captures tinyint, smallint, integer and
 * bigint.
 */
struct Integer : public Val {
  /** The raw integer value. */
  u64 val_;

  /**
   * Construct a non-NULL integer with the given value.
   * @param val The value to set.
   */
  explicit Integer(u64 val) noexcept : Val(false), val_(val) {}

  /**
   * @return A NULL integer.
   */
  static Integer Null() {
    Integer val(0);
    val.is_null_ = true;
    return val;
  }
};

/**
 * A NULL-able single- and double-precision floating point SQL value.
 */
struct Real : public Val { // TODO(Matt): should this be refactored to Double
                           // for consistency?
  /** The raw double value. */
  double val_;

  /**
   * Construct a non-NULL real value from a 32-bit floating point value.
   * @param val The initial value.
   */
  explicit Real(float val) noexcept : Val(false), val_(val) {}

  /**
   * Construct a non-NULL real value from a 64-bit floating point value
   * @param val The initial value.
   */
  explicit Real(double val) noexcept : Val(false), val_(val) {}

  /**
   * @return A NULL Real value.
   */
  static Real Null() {
    Real real(0.0);
    real.is_null_ = true;
    return real;
  }
};

template <typename T>
class Decimal {
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
  hash_t Hash(const hash_t seed) const { return shared::HashUtil::HashCrc(value_); }

  /**
   * @return The hash value of this decimal instance.
   */
  hash_t Hash() const { return Hash(0); }

  /**
   * Add the encoded decimal value @em that to this decimal value.
   * @param that The value to add.
   * @return This decimal value.
   */
  const Decimal<T> &operator+=(const T &that) {
    value_ += that;
    return *this;
  }

  /**
   * Subtract the encoded decimal value @em that from this decimal value.
   * @param that The value to subtract.
   * @return This decimal value.
   */
  const Decimal<T> &operator-=(const T &that) {
    value_ -= that;
    return *this;
  }

  /**
   * Multiply the encoded decimal value @em that with this decimal value.
   * @param that The value to multiply by.
   * @return This decimal value.
   */
  const Decimal<T> &operator*=(const T &that) {
    value_ *= that;
    return *this;
  }

  /**
   * Divide this decimal value by the encoded decimal value @em that.
   * @param that The value to divide by.
   * @return This decimal value.
   */
  const Decimal<T> &operator/=(const T &that) {
    value_ /= that;
    return *this;
  }

  /**
   * Modulo divide this decimal value by the encoded decimal value @em that.
   * @param that The value to modulus by.
   * @return This decimal value.
   */
  const Decimal<T> &operator%=(const T &that) {
    value_ %= that;
    return *this;
  }
};

using Decimal32 = Decimal<i32>;
using Decimal64 = Decimal<i64>;
using Decimal128 = Decimal<i128>;

/**
 * A NULL-able fixed-point decimal SQL value.
 */
struct DecimalVal : public Val {
  /** The internal decimal representation. */
  Decimal64 val_;

  /**
   * Construct a non-NULL decimal value from the given 64-bit decimal value.
   * @param val The decimal value.
   */
  explicit DecimalVal(Decimal64 val) noexcept : Val(false), val_(val) {}

  /**
   * Construct a non-NULL decimal value from the given 64-bit decimal value.
   * @param val The raw decimal value.
   */
  explicit DecimalVal(Decimal64::NativeType val) noexcept : DecimalVal(Decimal64{val}) {}

  /**
   * @return A NULL decimal value.
   */
  static DecimalVal Null() {
    DecimalVal val(0);
    val.is_null_ = true;
    return val;
  }
};

/**
 * A NULL-able SQL string. These strings are always <b>views</b> onto externally
 * managed memory. They never own the memory they point to! They're a very thin
 * wrapper around storage::VarlenEntry used for string processing.
 */
struct StringVal : public Val {
  /** The VarlenEntry being wrapped. */
  std::string val_;

  /**
   * Construct a non-NULL string from the given string value.
   * @param val The string.
   */
  explicit StringVal(std::string val) noexcept : Val(false), val_(val) {}

  /**
   * Create a non-NULL string value (i.e., a view) over the given (potentially
   * non-null terminated) string.
   * @param str The character sequence.
   * @param len The length of the sequence.
   */
  StringVal(const char *str, uint32_t len) noexcept : Val(false), val_(std::string(str, len)) {
    DB7_ASSERT(str != nullptr, "String input cannot be NULL");
  }

  /**
   * @return std::string_view of StringVal's contents
   */
  std::string_view StringView() const {
    assert(!is_null_ && "You should be doing a NULL check before attempting to "
                        "generate a std::string_view of a StringVal.");
    return std::string_view(val_);
  }

  /**
   * Create a non-NULL string value (i.e., view) over the C-style
   * null-terminated string.
   * @param str The C-string.
   */
  explicit StringVal(const char *str) noexcept : StringVal(const_cast<char *>(str), strlen(str)) {}

  /**
   * Get the length of the string value.
   * @return The length of the string in bytes.
   */
  std::size_t GetLength() const noexcept { return val_.length(); }

  /**
   * Return a pointer to the bytes underlying the string.
   * @return A pointer to the underlying content.
   */
  const char *GetContent() const noexcept { return val_.c_str(); }

  /**
   * Compare if this (potentially nullable) string value is equivalent to
   * another string value, taking NULLness into account.
   * @param that The string value to compare with.
   * @return True if equivalent; false otherwise.
   */
  bool operator==(const StringVal &that) const {
    if (is_null_ != that.is_null_) { return false; }
    if (is_null_) { return true; }

    return std::strcmp(that.GetContent(), GetContent()) == 0;
  }

  /**
   * Is this string not equivalent to another?
   * @param that The string value to compare with.
   * @return True if not equivalent; false otherwise.
   */
  bool operator!=(const StringVal &that) const { return !(*this == that); }

  /**
   * @return A NULL varchar/string.
   */
  static StringVal Null() {
    StringVal result("");
    result.is_null_ = true;
    return result;
  }
};

} // namespace db7::parser