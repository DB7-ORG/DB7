#include "table_star_expression.hpp"

namespace db7::parser {

std::unique_ptr<AbstractExpression> TableStarExpression::Copy() const {
  auto expr = std::make_unique<TableStarExpression>();
  expr->SetMutableStateForCopy(*this);
  expr->target_table_specified_ = target_table_specified_;
  expr->target_table_ = target_table_;
  return expr;
}

// void
// TableStarExpression::Accept(shared::ManagedPointer<binder::SqlNodeVisitor> v)
// {
//   v->Visit(shared::ManagedPointer(this));
// }

DEFINE_JSON_BODY_DECLARATIONS(TableStarExpression);

} // namespace db7::parser
