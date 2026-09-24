#pragma once

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "abstract_expression.hpp"
#include "common.hpp"
#include "value.hpp"

namespace db7::parser {

/**
 * ConstantValueExpression represents a constant, e.g. numbers, string literals.
 */
class ConstantValueExpression : public AbstractExpression {
private:
  // friend class binder::BindNodeVisitor; /* value_ may be modified, e.g., when
  // parsing dates. */
  void Validate() const;
  std::variant<Val, BoolVal, Integer, Real, DecimalVal, StringVal> value_{Val(true)};
  std::unique_ptr<byte[]> buffer_ = nullptr;

public:
  /**
   * Construct a NULL CVE of provided type
   * @param type SQL type for NULL, apparently can be INVALID coming out of the
   * parser for NULLs
   */
  explicit ConstantValueExpression(const type_id type) : ConstantValueExpression(type, Val(true)) {
    Validate();
  }

  /**
   * Construct a CVE of provided type and value
   * @tparam T execution value type to copy from
   * @param type SQL type, apparently can be INVALID coming out of the parser
   * for NULLs
   * @param value underlying value to copy
   */
  template <typename T> ConstantValueExpression(type_id type, T value);

  /**
   * Construct a CVE of provided type and value
   * @param type SQL type, apparently can be INVALID coming out of the parser
   * for NULLs
   * @param value underlying value to copy
   * @param buffer StringVal might not be inlined, so take ownership of that
   * buffer
   */
  ConstantValueExpression(type_id type, StringVal value, std::unique_ptr<byte[]> buffer);

  /** Default constructor for deserialization. */
  ConstantValueExpression() = default;

  /**
   * Copy assignment operator
   * @param other CVE to copy
   * @return self-reference
   */
  ConstantValueExpression &operator=(const ConstantValueExpression &other);

  /**
   * Move assignment operator
   * @param other CVE to move
   * @return self-reference
   */
  ConstantValueExpression &operator=(ConstantValueExpression &&other) noexcept;

  /**
   * Move constructor
   * @param other CVE to move
   */
  ConstantValueExpression(ConstantValueExpression &&other) noexcept;

  /**
   * Copy constructor
   * @param other CVE to copy
   */
  ConstantValueExpression(const ConstantValueExpression &other);

  hash_t Hash() const override;

  bool operator==(const AbstractExpression &other) const override;

  /**
   * Copies this ConstantValueExpression
   * @returns copy of this
   */
  std::unique_ptr<AbstractExpression> Copy() const override {
    return std::unique_ptr<AbstractExpression>{std::make_unique<ConstantValueExpression>(*this)};
  }

  /**
   * Creates a copy of the current AbstractExpression with new children
   * implanted. The children should not be owned by any other
   * AbstractExpression.
   * @param children New children to be owned by the copy
   * @returns copy of this with new children
   */
  std::unique_ptr<AbstractExpression>
  CopyWithChildren(std::vector<std::unique_ptr<AbstractExpression>> &&children) const override {
    assert(children.empty() && "ConstantValueExpression should have 0 children");
    (void)children;
    return Copy();
  }

  void DeriveExpressionName() override {
    if (!this->GetAliasName().empty()) { this->SetExpressionName(this->GetAliasName()); }
  }

  /**
   * @return copy of the underlying Val
   */
  BoolVal GetBoolVal() const {
    assert(std::holds_alternative<BoolVal>(value_) && "Invalid variant type for Get.");
    return std::get<BoolVal>(value_);
  }

  /**
   * @return copy of the underlying Val
   */
  Integer GetInteger() const {
    assert(std::holds_alternative<Integer>(value_) && "Invalid variant type for Get.");
    return std::get<Integer>(value_);
  }

  /**
   * @return copy of the underlying Val
   */
  Real GetReal() const {
    assert(std::holds_alternative<Real>(value_) && "Invalid variant type for Get.");
    return std::get<Real>(value_);
  }

  /**
   * @return copy of underlying Val
   */
  DecimalVal GetDecimalVal() const {
    assert(std::holds_alternative<DecimalVal>(value_) && "Invalid variant type for Get.");
    return std::get<DecimalVal>(value_);
  }

  /**
   * @return copy of the underlying Val
   * @warning StringVal may not have inlined its value, in which case the
   * StringVal returned by this function will hold a pointer to the buffer in
   * this CVE. In that case, do not destroy this CVE before the copied StringVal
   */
  StringVal GetStringVal() const {
    assert(std::holds_alternative<StringVal>(value_) && "Invalid variant type for Get.");
    return std::get<StringVal>(value_);
  }

  /**
   * Change the underlying value of this CVE. Used by the BinderSherpa to
   * promote parameters
   * @param type SQL type, apparently can be INVALID coming out of the parser
   * for NULLs
   * @param value underlying value to copy
   * @param buffer StringVal might not be inlined, so take ownership of that
   * buffer
   */
  void SetValue(const type_id type, const StringVal value, std::unique_ptr<byte[]> buffer) {
    return_value_type_ = type;
    value_ = value;
    buffer_ = std::move(buffer);
    Validate();
  }

  /**
   * Change the underlying value of this CVE. Used by the BinderSherpa to
   * promote parameters
   * @tparam T execution value type to copy from
   * @param type SQL type, apparently can be INVALID coming out of the parser
   * for NULLs
   * @param value underlying value to copy
   */
  template <typename T> void SetValue(type_id type, T value) {
    return_value_type_ = type;
    value_ = value;
    buffer_ = nullptr;
    Validate();
  }

  /**
   * @return true if CVE value represents a NULL
   */
  bool IsNull() const {
    if (std::holds_alternative<Val>(value_) && std::get<Val>(value_).is_null_) return true;
    switch (return_value_type_) {
    case type_id::BOOLEAN: {
      return GetBoolVal().is_null_;
    }
    case type_id::TINYINT:
    case type_id::SMALLINT:
    case type_id::INTEGER:
    case type_id::BIGINT: {
      return GetInteger().is_null_;
    }
    case type_id::DOUBLE: {
      return GetReal().is_null_;
    }
    case type_id::VARCHAR:
    case type_id::VARBINARY: {
      return GetStringVal().is_null_;
    }
    default: assert(false && "Invalid TypeId."); __builtin_unreachable();
    }
  }

  /**
   * Extracts the underlying execution value as a C++ type
   * @tparam T C++ type to extract
   * @return copy of the underlying value as the requested type
   * @warning std::string_view returned by this function will hold a pointer to
   * the buffer in this CVE. In that case, do not destroy this CVE before the
   * std::string_view
   */
  template <typename T> T Peek() const;

  // void Accept(shared::ManagedPointer<binder::SqlNodeVisitor> v) override;

  /** @return A string representation of this ConstantValueExpression. */
  std::string ToString() const;

  /** @return A ConstantValueExpression from input string and type. */
  static ConstantValueExpression FromString(const std::string &val_string, type_id type_id);

  /**
   * @return expression serialized to json
   */
  nlohmann::json ToJson() const override;

  /**
   * @param j json to deserialize
   */
  std::vector<std::unique_ptr<AbstractExpression>> FromJson(const nlohmann::json &j) override;
};

DEFINE_JSON_HEADER_DECLARATIONS(ConstantValueExpression);

/// @cond DOXYGEN_IGNORE
extern template ConstantValueExpression::ConstantValueExpression(const type_id type,
                                                                 const Val value);
extern template ConstantValueExpression::ConstantValueExpression(const type_id type,
                                                                 const BoolVal value);
extern template ConstantValueExpression::ConstantValueExpression(const type_id type,
                                                                 const Integer value);
extern template ConstantValueExpression::ConstantValueExpression(const type_id type,
                                                                 const Real value);
extern template ConstantValueExpression::ConstantValueExpression(const type_id type,
                                                                 const DecimalVal value);
extern template ConstantValueExpression::ConstantValueExpression(const type_id type,
                                                                 const StringVal value);

extern template void ConstantValueExpression::SetValue(const type_id type, const Val value);
extern template void ConstantValueExpression::SetValue(const type_id type, const BoolVal value);
extern template void ConstantValueExpression::SetValue(const type_id type, const Integer value);
extern template void ConstantValueExpression::SetValue(const type_id type, const Real value);
extern template void ConstantValueExpression::SetValue(const type_id type, const DecimalVal value);
extern template void ConstantValueExpression::SetValue(const type_id type, const StringVal value);

extern template bool ConstantValueExpression::Peek() const;
extern template int8_t ConstantValueExpression::Peek() const;
extern template int16_t ConstantValueExpression::Peek() const;
extern template int32_t ConstantValueExpression::Peek() const;
extern template int64_t ConstantValueExpression::Peek() const;
extern template float ConstantValueExpression::Peek() const;
extern template double ConstantValueExpression::Peek() const;
extern template Decimal32 ConstantValueExpression::Peek() const;
extern template Decimal64 ConstantValueExpression::Peek() const;
extern template Decimal128 ConstantValueExpression::Peek() const;
extern template std::string_view ConstantValueExpression::Peek() const;
extern template std::string ConstantValueExpression::Peek() const;
/// @endcond

} // namespace db7::parser