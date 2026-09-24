#include "star_expression.hpp"

namespace db7::parser {

std::unique_ptr<AbstractExpression> StarExpression::Copy() const {
  auto expr = std::make_unique<StarExpression>();
  expr->SetMutableStateForCopy(*this);
  return expr;
}

// void StarExpression::Accept(shared::ManagedPointer<binder::SqlNodeVisitor> v)
// {
//     v->Visit(shared::ManagedPointer(this));
// }

DEFINE_JSON_BODY_DECLARATIONS(StarExpression);

} // namespace db7::parser
