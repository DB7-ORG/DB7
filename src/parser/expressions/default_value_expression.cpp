#include "parser/expressions/default_value_expression.hpp"

namespace db7::parser {
std::unique_ptr<AbstractExpression> DefaultValueExpression::Copy() const {
  auto expr = std::make_unique<DefaultValueExpression>();
  expr->SetMutableStateForCopy(*this);
  return expr;
}

// void
// DefaultValueExpression::Accept(shared::ManagedPointer<binder::SqlNodeVisitor>
// v)
// {
//     v->Visit(shared::ManagedPointer(this));
// }

DEFINE_JSON_BODY_DECLARATIONS(DefaultValueExpression);

} // namespace db7::parser