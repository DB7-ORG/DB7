#include "constant_value_expression.hpp"
#include "value_util.hpp"

#include <fmt/format.h>

namespace db7::parser {

template <typename T>
ConstantValueExpression::ConstantValueExpression(const access::type_id type,
                                                 const T value)
    : AbstractExpression(ExpressionType::VALUE_CONSTANT, type, {}),
      value_(value) {
  Validate();
}

ConstantValueExpression::ConstantValueExpression(const access::type_id type,
                                                 const StringVal value,
                                                 std::unique_ptr<byte[]> buffer)
    : AbstractExpression(ExpressionType::VALUE_CONSTANT, type, {}),
      value_(value), buffer_(std::move(buffer)) {
  Validate();
}

void ConstantValueExpression::Validate() const {
  if (std::holds_alternative<Val>(value_)) {
    assert(std::get<Val>(value_).is_null_ &&
           "Should have only constructed a base-type Val in the event of a "
           "NULL (likely coming out of PostgresParser).");
  } else if (std::holds_alternative<BoolVal>(value_)) {
    assert(return_value_type_ == access::type_id::BOOLEAN &&
           "Invalid TypeId for Val type.");
  } else if (std::holds_alternative<Integer>(value_)) {
    assert((return_value_type_ == access::type_id::TINYINT ||
            return_value_type_ == access::type_id::SMALLINT ||
            return_value_type_ == access::type_id::INTEGER ||
            return_value_type_ == access::type_id::BIGINT) &&
           "Invalid TypeId for Val type.");
  } else if (std::holds_alternative<Real>(value_)) {
    assert(return_value_type_ == access::type_id::DOUBLE &&
           "Invalid TypeId for Val type.");
  } else if (std::holds_alternative<StringVal>(value_)) {
    assert((return_value_type_ == access::type_id::VARCHAR ||
            return_value_type_ == access::type_id::VARBINARY) &&
           "Invalid TypeId for Val type.");
  } else {
    __builtin_unreachable();
  }
}

template <typename T> T ConstantValueExpression::Peek() const {
  // NOLINTNEXTLINE: bugprone-suspicious-semicolon: seems like a false positive
  // because of constexpr
  if constexpr (std::is_same_v<T, bool>) {
    return static_cast<T>(GetBoolVal().val_);
  }
  // NOLINTNEXTLINE: bugprone-suspicious-semicolon: seems like a false positive
  // because of constexpr
  if constexpr (std::is_same_v<T, i8> || std::is_same_v<T, i16> ||
                std::is_same_v<T, i32> ||
                std::is_same_v<T,
                               i64>) { // NOLINT: bugprone-suspicious-semicolon:
                                       // seems like a false positive
    // because of constexpr
    return static_cast<T>(GetInteger().val_);
  }
  // NOLINTNEXTLINE: bugprone-suspicious-semicolon: seems like a false positive
  // because of constexpr
  if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
    return static_cast<T>(GetReal().val_);
  }
  // NOLINTNEXTLINE: bugprone-suspicious-semicolon: seems like a false positive
  // because of constexpr
  if constexpr (std::is_same_v<T, Decimal32> || std::is_same_v<T, Decimal64> ||
                std::is_same_v<T,

                               Decimal128>) { // NOLINT:
                                              // bugprone-suspicious-semicolon:
                                              // seems like a
                                              // false positive
    return static_cast<T>(GetDecimalVal().val_);
  }
  // NOLINTNEXTLINE: bugprone-suspicious-semicolon: seems like a false positive
  // because of constexpr
  if constexpr (std::is_same_v<T, std::string_view>) {
    return std::get<StringVal>(value_).StringView();
  }
  // NOLINTNEXTLINE: bugprone-suspicious-semicolon: seems like a false positive
  // because of constexpr
  if constexpr (std::is_same_v<T, std::string>) {
    return GetStringVal().val_;
  }
  __builtin_unreachable();
}

ConstantValueExpression &
ConstantValueExpression::operator=(const ConstantValueExpression &other) {
  if (this != &other) { // self-assignment check expected
    // AbstractExpression fields we need copied over
    expression_type_ = other.expression_type_;
    expression_name_ = other.expression_name_;
    alias_ = other.alias_;
    return_value_type_ = other.return_value_type_;
    depth_ = other.depth_;
    has_subquery_ = other.has_subquery_;
    // ConstantValueExpression fields
    if (std::holds_alternative<StringVal>(other.value_)) {
      auto string_val = ValueUtil::CreateStringVal(other.GetStringVal());

      value_ = string_val.first;
      buffer_ = std::move(string_val.second);
    } else {
      value_ = other.value_;
      buffer_ = nullptr;
    }
  }
  Validate();
  return *this;
}

ConstantValueExpression::ConstantValueExpression(
    const ConstantValueExpression &other)
    : AbstractExpression(other) {
  if (std::holds_alternative<StringVal>(other.value_)) {
    auto string_val = ValueUtil::CreateStringVal(other.GetStringVal());

    value_ = string_val.first;
    buffer_ = std::move(string_val.second);
  } else {
    value_ = other.value_;
  }
  Validate();
}

ConstantValueExpression &
ConstantValueExpression::operator=(ConstantValueExpression &&other) noexcept {
  if (this != &other) { // self-assignment check expected
    // AbstractExpression fields we need moved over
    expression_type_ = other.expression_type_;
    expression_name_ = std::move(other.expression_name_);
    alias_ = std::move(other.alias_);
    return_value_type_ = other.return_value_type_;
    depth_ = other.depth_;
    has_subquery_ = other.has_subquery_;
    // ConstantValueExpression fields
    value_ = other.value_;
    buffer_ = std::move(other.buffer_);
    // Set other to NULL because unclear what else it would be in this case
    other.value_ = Val(true);
  }
  Validate();
  return *this;
}

ConstantValueExpression::ConstantValueExpression(
    ConstantValueExpression &&other) noexcept
    : AbstractExpression(other) {
  value_ = other.value_;
  buffer_ = std::move(other.buffer_);
  // Set other to NULL because unclear what else it would be in this case
  other.value_ = Val(true);
  Validate();
}

hash_t ConstantValueExpression::Hash() const {
  const auto hash = shared::HashUtil::CombineHashes(
      AbstractExpression::Hash(), shared::HashUtil::Hash(IsNull()));
  if (IsNull())
    return hash;

  switch (GetReturnValueType()) {
  case access::type_id::BOOLEAN: {
    return shared::HashUtil::CombineHashes(
        hash, shared::HashUtil::Hash(Peek<bool>()));
  }
  case access::type_id::TINYINT:
  case access::type_id::SMALLINT:
  case access::type_id::INTEGER:
  case access::type_id::BIGINT: {
    return shared::HashUtil::CombineHashes(hash,
                                           shared::HashUtil::Hash(Peek<i64>()));
  }
  case access::type_id::DOUBLE: {
    return shared::HashUtil::CombineHashes(
        hash, shared::HashUtil::Hash(Peek<double>()));
  }
  case access::type_id::VARCHAR:
  case access::type_id::VARBINARY: {
    return shared::HashUtil::CombineHashes(
        hash, shared::HashUtil::Hash(Peek<std::string_view>()));
  }
  default:
    __builtin_unreachable();
  }
}

bool ConstantValueExpression::operator==(
    const AbstractExpression &other) const {
  if (!AbstractExpression::operator==(other))
    return false;
  const auto &other_cve = dynamic_cast<const ConstantValueExpression &>(other);

  if (IsNull() != other_cve.IsNull())
    return false;
  if (IsNull() && other_cve.IsNull())
    return true;

  switch (other.GetReturnValueType()) {
  case access::type_id::BOOLEAN: {
    return Peek<bool>() == other_cve.Peek<bool>();
  }
  case access::type_id::TINYINT:
  case access::type_id::SMALLINT:
  case access::type_id::INTEGER:
  case access::type_id::BIGINT: {
    return Peek<i64>() == other_cve.Peek<i64>();
  }
  case access::type_id::DOUBLE: {
    return Peek<double>() == other_cve.Peek<double>();
  }
  case access::type_id::VARCHAR:
  case access::type_id::VARBINARY: {
    return Peek<std::string_view>() == other_cve.Peek<std::string_view>();
  }
  default:
    __builtin_unreachable();
  }
}

std::string ConstantValueExpression::ToString() const {
  switch (GetReturnValueType()) {
  case access::type_id::BOOLEAN: {
    return fmt::format("{}", GetBoolVal().val_);
  }
  case access::type_id::TINYINT:
  case access::type_id::SMALLINT:
  case access::type_id::INTEGER:
  case access::type_id::BIGINT: {
    return fmt::format("{}", GetInteger().val_);
  }
  case access::type_id::DOUBLE: {
    return fmt::format("{}", GetReal().val_);
  }
  case access::type_id::VARCHAR:
  case access::type_id::VARBINARY: {
    return fmt::format("{}", GetStringVal().val_);
  }
  default:
    __builtin_unreachable();
  }
}

ConstantValueExpression
ConstantValueExpression::FromString(const std::string &val,
                                    access::type_id type_id) {
  if (val.empty())
    return ConstantValueExpression(type_id);
  switch (type_id) {
  case access::type_id::BOOLEAN: {
    return ConstantValueExpression(type_id, BoolVal(std::stoi(val) != 0));
  }
  case access::type_id::TINYINT:
  case access::type_id::SMALLINT:
  case access::type_id::INTEGER:
  case access::type_id::BIGINT: {
    return ConstantValueExpression(type_id, Integer(std::stoll(val)));
  }
  case access::type_id::DOUBLE: {
    return ConstantValueExpression(type_id, Real(std::stod(val)));
  }
  case access::type_id::VARCHAR:
  case access::type_id::VARBINARY: {
    auto string_val = ValueUtil::CreateStringVal(val);
    return ConstantValueExpression(type_id, string_val.first,
                                   std::move(string_val.second));
  }
  default:
    __builtin_unreachable();
  }
}

nlohmann::json ConstantValueExpression::ToJson() const {
  nlohmann::json j = AbstractExpression::ToJson();

  if (!IsNull()) {
    switch (return_value_type_) {
    case access::type_id::BOOLEAN: {
      j["value"] = Peek<bool>();
      break;
    }
    case access::type_id::TINYINT:
    case access::type_id::SMALLINT:
    case access::type_id::INTEGER:
    case access::type_id::BIGINT: {
      j["value"] = Peek<i64>();
      break;
    }
    case access::type_id::DOUBLE: {
      j["value"] = Peek<double>();
      break;
    }
    case access::type_id::VARCHAR:
    case access::type_id::VARBINARY: {
      std::string val{Peek<std::string_view>()};
      j["value"] = val;
      break;
    }
    default:
      __builtin_unreachable();
    }
  }
  return j;
}

std::vector<std::unique_ptr<AbstractExpression>>
ConstantValueExpression::FromJson(const nlohmann::json &j) {
  std::vector<std::unique_ptr<AbstractExpression>> exprs;
  auto e1 = AbstractExpression::FromJson(j);
  exprs.insert(exprs.end(), std::make_move_iterator(e1.begin()),
               std::make_move_iterator(e1.end()));

  if (j.find("value") != j.end()) {
    // it's not NULL
    switch (return_value_type_) {
    case access::type_id::BOOLEAN: {
      value_ = BoolVal(j.at("value").get<bool>());
      break;
    }
    case access::type_id::TINYINT:
    case access::type_id::SMALLINT:
    case access::type_id::INTEGER:
    case access::type_id::BIGINT: {
      value_ = Integer(j.at("value").get<i64>());
      break;
    }
    case access::type_id::DOUBLE: {
      value_ = Real(j.at("value").get<double>());
      break;
    }
    case access::type_id::VARCHAR:
    case access::type_id::VARBINARY: {
      auto string_val =
          ValueUtil::CreateStringVal(j.at("value").get<std::string>());

      value_ = string_val.first;
      buffer_ = std::move(string_val.second);

      break;
    }
    default:
      __builtin_unreachable();
    }
  } else {
    value_ = Val(true);
  }

  Validate();

  return exprs;
}

// void
// ConstantValueExpression::Accept(shared::ManagedPointer<binder::SqlNodeVisitor>
// v)
// {
//     v->Visit(shared::ManagedPointer(this));
// }

DEFINE_JSON_BODY_DECLARATIONS(ConstantValueExpression);

template ConstantValueExpression::ConstantValueExpression(
    const access::type_id type, const Val value);
template ConstantValueExpression::ConstantValueExpression(
    const access::type_id type, const BoolVal value);
template ConstantValueExpression::ConstantValueExpression(
    const access::type_id type, const Integer value);
template ConstantValueExpression::ConstantValueExpression(
    const access::type_id type, const Real value);
template ConstantValueExpression::ConstantValueExpression(
    const access::type_id type, const DecimalVal value);
template ConstantValueExpression::ConstantValueExpression(
    const access::type_id type, const StringVal value);

template void ConstantValueExpression::SetValue(const access::type_id type,
                                                const Val value);
template void ConstantValueExpression::SetValue(const access::type_id type,
                                                const BoolVal value);
template void ConstantValueExpression::SetValue(const access::type_id type,
                                                const Integer value);
template void ConstantValueExpression::SetValue(const access::type_id type,
                                                const Real value);
template void ConstantValueExpression::SetValue(const access::type_id type,
                                                const DecimalVal value);
template void ConstantValueExpression::SetValue(const access::type_id type,
                                                const StringVal value);

template bool ConstantValueExpression::Peek() const;
template i8 ConstantValueExpression::Peek() const;
template i16 ConstantValueExpression::Peek() const;
template i32 ConstantValueExpression::Peek() const;
template i64 ConstantValueExpression::Peek() const;
template float ConstantValueExpression::Peek() const;
template double ConstantValueExpression::Peek() const;
template Decimal32 ConstantValueExpression::Peek() const;
template Decimal64 ConstantValueExpression::Peek() const;
template Decimal128 ConstantValueExpression::Peek() const;
template std::string_view ConstantValueExpression::Peek() const;
template std::string ConstantValueExpression::Peek() const;

} // namespace db7::parser