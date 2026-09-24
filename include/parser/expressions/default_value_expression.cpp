#include "default_value_expression.hpp"

namespace noisepage::parser
{
    std::unique_ptr<AbstractExpression> DefaultValueExpression::Copy() const
    {
        auto expr = std::make_unique<DefaultValueExpression>();
        expr->SetMutableStateForCopy(*this);
        return expr;
    }

    // void DefaultValueExpression::Accept(common::ManagedPointer<binder::SqlNodeVisitor> v)
    // {
    //     v->Visit(common::ManagedPointer(this));
    // }

    DEFINE_JSON_BODY_DECLARATIONS(DefaultValueExpression);

} // namespace noisepage::parser