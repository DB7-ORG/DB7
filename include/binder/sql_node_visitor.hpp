#pragma once

#include "shared/managed_pointer.hpp"

namespace db7 {

namespace parser {
class ParseResult;

class SelectStatement;
class CreateStatement;
class CreateFunctionStatement;
class InsertStatement;
class DeleteStatement;
class DropStatement;
class ExplainStatement;
class PrepareStatement;
class ExecuteStatement;
class TransactionStatement;
class UpdateStatement;
class CopyStatement;
class AnalyzeStatement;
class VariableSetStatement;
class VariableShowStatement;
class JoinDefinition;
class TableRef;

class GroupByDescription;
class OrderByDescription;
class LimitDescription;

class AggregateExpression;
class CaseExpression;
class ColumnValueExpression;
class ComparisonExpression;
class ConjunctionExpression;
class ConstantValueExpression;
class DefaultValueExpression;
class DerivedValueExpression;
class FunctionExpression;
class OperatorExpression;
class ParameterValueExpression;
class StarExpression;
class TableStarExpression;
class SubqueryExpression;
class TypeCastExpression;
} // namespace parser

namespace binder {

/**
 * Visitor pattern definitions for the parser statements.
 */
class SqlNodeVisitor {
public:
  /**
   * Virtual destructor for SqlNodeVisitor.
   */
  virtual ~SqlNodeVisitor() = default;

  /**
   * Visitor pattern for AnalyzeStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::AnalyzeStatement> node) {}

  /**
   * Visitor pattern for CopyStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::CopyStatement> node) {}

  /**
   * Visitor pattern for CreateFunctionStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::CreateFunctionStatement> node) {}

  /**
   * Visitor pattern for CreateStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::CreateStatement> node) {}

  /**
   * Visitor pattern for DeleteStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::DeleteStatement> node) {}

  /**
   * Visitor pattern for DropStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::DropStatement> node) {}

  /**
   * Visitor pattern for ExecuteStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::ExecuteStatement> node) {}

  /**
   * Visitor pattern for ExplainStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::ExplainStatement> node) {}

  /**
   * Visitor pattern for InsertStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::InsertStatement> node) {}

  /**
   * Visitor pattern for PrepareStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::PrepareStatement> node) {}

  /**
   * Visitor pattern for SelectStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::SelectStatement> node) {}

  /**
   * Visitor pattern for TransactionStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::TransactionStatement> node) {}

  /**
   * Visitor pattern for UpdateStatement.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::UpdateStatement> node) {}

  /**
   * Visitor pattern for VariableSetStatement
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::VariableSetStatement> node) {}

  /**
   * Visitor pattern for VariableShowStatement
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::VariableShowStatement> node) {}

  /**
   * Visitor pattern for AggregateExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::AggregateExpression> expr);

  /**
   * Visitor pattern for CaseExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::CaseExpression> expr);

  /**
   * Visitor pattern for ColumnValueExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::ColumnValueExpression> expr);

  /**
   * Visitor pattern for ComparisonExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::ComparisonExpression> expr);

  /**
   * Visitor pattern for ConjunctionExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::ConjunctionExpression> expr);

  /**
   * Visitor pattern for ConstantValueExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::ConstantValueExpression> expr);

  /**
   * Visitor pattern for DefaultValueExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::DefaultValueExpression> expr);

  /**
   * Visitor pattern for DerivedValueExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::DerivedValueExpression> expr);

  /**
   * Visitor pattern for FunctionExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::FunctionExpression> expr);

  /**
   * Visitor pattern for OperatorExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::OperatorExpression> expr);

  /**
   * Visitor pattern for ParameterValueExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::ParameterValueExpression> expr);

  /**
   * Visitor pattern for StarExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::StarExpression> expr);

  /**
   * Visitor pattern for TableStarExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::TableStarExpression> expr);

  /**
   * Visitor pattern for SubqueryExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::SubqueryExpression> expr);

  /**
   * Visitor pattern for TypeCastExpression
   * @param expr to be visited
   */
  virtual void Visit(ManagedPointer<parser::TypeCastExpression> expr);

  // START some sub query nodes inside SelectStatement

  /**
   * Visitor pattern for GroupByDescription.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::GroupByDescription> node) {}

  /**
   * Visitor pattern for JoinDefinition.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::JoinDefinition> node) {}

  /**
   * Visitor pattern for LimitDescription.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::LimitDescription> node) {}

  /**
   * Visitor pattern for OrderByDescription.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::OrderByDescription> node) {}

  /**
   * Visitor pattern for TableRef.
   * @param node node to be visited
   */
  virtual void Visit(ManagedPointer<parser::TableRef> node) {}

  // END some sub query nodes inside SelectStatement
};

} // namespace binder
} // namespace db7
