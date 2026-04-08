#pragma once

#include "common.hpp"
#include "managed_pointer.hpp"
#include "expressions/abstract_expression.hpp"
#include <vector>

namespace noisepage::parser
{
    enum OrderType
    {
        kOrderAsc,
        kOrderDesc
    };

    class OrderByDescription
    {
        std::vector<OrderType> types_;
        std::vector<common::ManagedPointer<AbstractExpression>> exprs_;
    };

}
