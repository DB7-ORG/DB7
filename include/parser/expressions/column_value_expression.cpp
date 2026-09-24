#include "column_value_expression.hpp"
#include "shared/hash_util.hpp"

namespace db7::parser {
std::unique_ptr<AbstractExpression> ColumnValueExpression::Copy() const {
  auto expr = std::make_unique<ColumnValueExpression>(
      GetDatabaseOid(), GetTableOid(), GetColumnOid());
  expr->SetMutableStateForCopy(*this);
  expr->table_alias_ = this->table_alias_;
  expr->column_name_ = this->column_name_;
  expr->SetDatabaseOID(this->database_oid_);
  expr->SetTableOID(this->table_oid_);
  expr->SetColumnOID(this->column_oid_);
  return expr;
}

hash_t ColumnValueExpression::Hash() const {
  hash_t hash = shared::HashUtil::Hash(GetExpressionType());
  hash = shared::HashUtil::CombineHashes(
      hash, shared::HashUtil::Hash(GetReturnValueType()));
  hash = shared::HashUtil::CombineHashes(
      hash, std::hash<parser::AliasType>{}(table_alias_));
  hash = shared::HashUtil::CombineHashes(hash,
                                         shared::HashUtil::Hash(column_name_));
  hash = shared::HashUtil::CombineHashes(hash,
                                         shared::HashUtil::Hash(database_oid_));
  hash =
      shared::HashUtil::CombineHashes(hash, shared::HashUtil::Hash(table_oid_));
  hash = shared::HashUtil::CombineHashes(hash,
                                         shared::HashUtil::Hash(column_oid_));
  hash = shared::HashUtil::CombineHashes(hash, std::hash<AliasType>{}(alias_));
  return hash;
}

bool ColumnValueExpression::operator==(const AbstractExpression &rhs) const {
  if (GetExpressionType() != rhs.GetExpressionType())
    return false;
  if (GetReturnValueType() != rhs.GetReturnValueType())
    return false;

  auto const &other = dynamic_cast<const ColumnValueExpression &>(rhs);
  if (GetColumnName() != other.GetColumnName())
    return false;
  if (GetTableAlias() != other.GetTableAlias())
    return false;
  if (GetColumnOid() != other.GetColumnOid())
    return false;
  if (GetTableOid() != other.GetTableOid())
    return false;
  if (!(GetAlias() == rhs.GetAlias()))
    return false;
  return GetDatabaseOid() == other.GetDatabaseOid();
}

void ColumnValueExpression::DeriveExpressionName() {
  if (!(this->GetAlias().Empty()))
    this->SetExpressionName(this->GetAlias().GetName());
  else
    this->SetExpressionName(column_name_);
}

nlohmann::json ColumnValueExpression::ToJson() const {
  nlohmann::json j = AbstractExpression::ToJson();
  j["table_name"] = table_alias_.ToJson();
  j["column_name"] = column_name_;
  j["database_oid"] = database_oid_;
  j["table_oid"] = table_oid_;
  j["column_oid"] = column_oid_;
  return j;
}

std::vector<std::unique_ptr<AbstractExpression>>
ColumnValueExpression::FromJson(const nlohmann::json &j) {
  std::vector<std::unique_ptr<AbstractExpression>> exprs;
  auto e1 = AbstractExpression::FromJson(j);
  exprs.insert(exprs.end(), std::make_move_iterator(e1.begin()),
               std::make_move_iterator(e1.end()));
  table_alias_.FromJson(j.at("table_name"));
  column_name_ = j.at("column_name").get<std::string>();
  database_oid_ = j.at("database_oid").get<catalog::db_oid_t>();
  table_oid_ = j.at("table_oid").get<catalog::rel_oid_t>();
  column_oid_ = j.at("column_oid").get<catalog::col_oid_t>();
  return exprs;
}

DEFINE_JSON_BODY_DECLARATIONS(ColumnValueExpression);
} // namespace db7::parser