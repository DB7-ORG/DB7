#include "shared/error/exception.hpp"

#include "parser/postgres_parser.hpp"

#include "parser/create_function_statement.hpp"
#include "parser/create_statement.hpp"
#include "parser/expressions/aggregate_expression.hpp"
#include "parser/expressions/case_expression.hpp"
#include "parser/expressions/column_value_expression.hpp"
#include "parser/expressions/comparison_expression.hpp"
#include "parser/expressions/conjuction_expression.hpp"
#include "parser/expressions/constant_value_expression.hpp"
#include "parser/expressions/default_value_expression.hpp"
#include "parser/expressions/function_expression.hpp"
#include "parser/expressions/operator_expression.hpp"
#include "parser/expressions/parameter_value_expression.hpp"
#include "parser/expressions/star_expression.hpp"
#include "parser/expressions/subquery_expression.hpp"
#include "parser/expressions/table_star_expression.hpp"
#include "parser/expressions/type_cast_expression.hpp"
#include "parser/expressions/value_util.hpp"

#include "parser/pg_node.hpp"

#include <fmt/format.h>
#include <unordered_set>

namespace db7::parser {

// Logs and throws for unsupported parse node types or enum values.
// Takes int so Postgres enums/NodeTags can be passed with static_cast<int>.
[[noreturn]] static void ThrowUnsupported(const char *fn, const char *what, int value) {
  fmt::print("{}: {} {} unsupported\n", fn, what, value);
  throw PARSER_EXCEPTION(fmt::format("{}: {} unsupported", fn, what));
}

std::unique_ptr<parser::ParseResult>
PostgresParser::BuildParseTree(const std::string &query_string) {
  // Frees all parse nodes when this function exits, including on exceptions
  struct PgMemoryContext {
    MemoryContext ctx = pg_query_enter_memory_context();
    ~PgMemoryContext() { pg_query_exit_memory_context(ctx); }
  } pg_ctx;

  PgQueryInternalParsetreeAndError result =
      pg_query_raw_parse(query_string.c_str(), PG_QUERY_PARSE_DEFAULT);

  if (result.stderr_buffer != nullptr) { free(result.stderr_buffer); }

  if (result.error != nullptr) {
    fmt::print("BuildParseTree error: msg {}, curpos {}", result.error->message,
               result.error->cursorpos);
    ParserException exception(std::string(result.error->message), __FILE__, __LINE__,
                              result.error->cursorpos);
    pg_query_free_error(result.error);
    throw exception;
  }

  auto parse_result = std::make_unique<ParseResult>();
  ListTransform(parse_result.get(), result.tree);
  return parse_result;
}

void PostgresParser::ListTransform(ParseResult *parse_result, List *root) {
  if (root != nullptr) {
    for (int i = 0; i < list_length(root); i++) {
      auto *raw = static_cast<RawStmt *>(list_nth(root, i));
      parse_result->AddStatement(NodeTransform(parse_result, raw->stmt));
    }
  }
}

std::unique_ptr<SQLStatement> PostgresParser::NodeTransform(ParseResult *parse_result, Node *node) {
  // TODO(WAN): Document what input is parsed to nullptr
  if (node == nullptr) { return nullptr; }

  std::unique_ptr<SQLStatement> result;
  switch (node->type) {
  // case T_CopyStmt: {
  //   result = CopyTransform(parse_result, reinterpret_cast<CopyStmt *>(node));
  //   break;
  // }
  case T_CreateStmt: {
    result = CreateTransform(parse_result, reinterpret_cast<CreateStmt *>(node));
    break;
  }
  case T_CreatedbStmt: {
    result = CreateDatabaseTransform(parse_result, reinterpret_cast<CreatedbStmt *>(node));
    break;
  }
  case T_CreateFunctionStmt: {
    result = CreateFunctionTransform(parse_result, reinterpret_cast<CreateFunctionStmt *>(node));
    break;
  }
  case T_CreateSchemaStmt: {
    result = CreateSchemaTransform(parse_result, reinterpret_cast<CreateSchemaStmt *>(node));
    break;
  }
  case T_CreateTrigStmt: {
    result = CreateTriggerTransform(parse_result, reinterpret_cast<CreateTrigStmt *>(node));
    break;
  }
  // case T_DropdbStmt: {
  //   result = DropDatabaseTransform(parse_result,
  //                                  reinterpret_cast<DropDatabaseStmt
  //                                  *>(node));
  //   break;
  // }
  // case T_DropStmt: {
  //   result = DropTransform(parse_result, reinterpret_cast<DropStmt *>(node));
  //   break;
  // }
  // case T_ExecuteStmt: {
  //   result =
  //       ExecuteTransform(parse_result, reinterpret_cast<ExecuteStmt
  //       *>(node));
  //   break;
  // }
  // case T_ExplainStmt: {
  //   result =
  //       ExplainTransform(parse_result, reinterpret_cast<ExplainStmt
  //       *>(node));
  //   break;
  // }
  case T_IndexStmt: {
    result = CreateIndexTransform(parse_result, reinterpret_cast<IndexStmt *>(node));
    break;
  }
  // case T_InsertStmt: {
  //   result =
  //       InsertTransform(parse_result, reinterpret_cast<InsertStmt *>(node));
  //   break;
  // }
  // case T_PrepareStmt: {
  //   result =
  //       PrepareTransform(parse_result, reinterpret_cast<PrepareStmt
  //       *>(node));
  //   break;
  // }
  case T_SelectStmt: {
    result = SelectTransform(parse_result, reinterpret_cast<SelectStmt *>(node));
    break;
  }
  // case T_VacuumStmt: {
  //   result =
  //       VacuumTransform(parse_result, reinterpret_cast<VacuumStmt *>(node));
  //   break;
  // }
  // case T_VariableSetStmt: {
  //   result = VariableSetTransform(parse_result,
  //                                 reinterpret_cast<VariableSetStmt *>(node));
  //   break;
  // }
  // case T_VariableShowStmt: {
  //   result = VariableShowTransform(parse_result,
  //                                  reinterpret_cast<VariableShowStmt
  //                                  *>(node));
  //   break;
  // }
  case T_ViewStmt: {
    result = CreateViewTransform(parse_result, reinterpret_cast<ViewStmt *>(node));
    break;
  }
  // case T_TruncateStmt: {
  //   result =
  //       TruncateTransform(parse_result, reinterpret_cast<TruncateStmt
  //       *>(node));
  //   break;
  // }
  // case T_TransactionStmt: {
  //   result = TransactionTransform(reinterpret_cast<TransactionStmt *>(node));
  //   break;
  // }
  // case T_UpdateStmt: {
  //   result =
  //       UpdateTransform(parse_result, reinterpret_cast<UpdateStmt *>(node));
  //   break;
  // }
  // case T_DeleteStmt: {
  //   result =
  //       DeleteTransform(parse_result, reinterpret_cast<DeleteStmt *>(node));
  //   break;
  // }
  default: {
    fmt::print("NodeTransform: statement type {} unsupported\n", static_cast<int>(node->type));
    throw PARSER_EXCEPTION("NodeTransform: unsupported statement type");
  }
  }
  return result;
}

std::unique_ptr<AbstractExpression> PostgresParser::ExprTransform(ParseResult *parse_result,
                                                                  Node *node, char *alias) {
  if (node == nullptr) { return nullptr; }

  std::unique_ptr<AbstractExpression> expr;
  switch (node->type) {
  case T_A_Const: {
    expr = ConstTransform(parse_result, reinterpret_cast<A_Const *>(node));
    break;
  }
  case T_A_Expr: {
    expr = AExprTransform(parse_result, reinterpret_cast<A_Expr *>(node));
    break;
  }
  case T_BoolExpr: {
    expr = BoolExprTransform(parse_result, reinterpret_cast<BoolExpr *>(node));
    break;
  }
  case T_CaseExpr: {
    expr = CaseExprTransform(parse_result, reinterpret_cast<CaseExpr *>(node));
    break;
  }
  case T_ColumnRef: {
    expr = ColumnRefTransform(parse_result, reinterpret_cast<ColumnRef *>(node), alias);
    break;
  }
  case T_FuncCall: {
    expr = FuncCallTransform(parse_result, reinterpret_cast<FuncCall *>(node));
    break;
  }
  case T_NullTest: {
    expr = NullTestTransform(parse_result, reinterpret_cast<NullTest *>(node));
    break;
  }
  case T_ParamRef: {
    expr = ParamRefTransform(parse_result, reinterpret_cast<ParamRef *>(node));
    break;
  }
  case T_SubLink: {
    expr = SubqueryExprTransform(parse_result, reinterpret_cast<SubLink *>(node));
    break;
  }
  case T_TypeCast: {
    expr = AExprTransform(parse_result, reinterpret_cast<A_Expr *>(node));
    break;
  }
  case T_Integer: {
    expr = std::make_unique<ConstantValueExpression>(type_id::INTEGER, Integer(PgIntVal(node)));
    break;
  }
  default: {
    fmt::print("ExprTransform: type {} unsupported", static_cast<int>(node->type));
    throw PARSER_EXCEPTION("ExprTransform: unsupported type");
  }
  }
  if (alias != nullptr) {
    expr->SetAlias(parser::AliasType(
        alias, alias_oid_t(reinterpret_cast<size_t>(reinterpret_cast<void *>(expr.get())))));
  }
  return expr;
}

/**
 * DO NOT USE THIS UNLESS YOU MUST.
 * Converts the Postgres parser's expression into our own expression type.
 * @param parser_str string representation returned by postgres parser
 * @return expression type corresponding to the string
 */
ExpressionType PostgresParser::StringToExpressionType(const std::string &parser_str) {
  std::string str = parser_str;
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);
  if (str == "OPERATOR_UNARY_MINUS") { return ExpressionType::OPERATOR_UNARY_MINUS; }
  if (str == "OPERATOR_PLUS" || str == "+") { return ExpressionType::OPERATOR_PLUS; }
  if (str == "OPERATOR_MINUS" || str == "-") { return ExpressionType::OPERATOR_MINUS; }
  if (str == "OPERATOR_MULTIPLY" || str == "*") { return ExpressionType::OPERATOR_MULTIPLY; }
  if (str == "OPERATOR_DIVIDE" || str == "/") { return ExpressionType::OPERATOR_DIVIDE; }
  if (str == "OPERATOR_CONCAT" || str == "||") { return ExpressionType::OPERATOR_CONCAT; }
  if (str == "OPERATOR_MOD" || str == "%") { return ExpressionType::OPERATOR_MOD; }
  if (str == "OPERATOR_NOT") { return ExpressionType::OPERATOR_NOT; }
  if (str == "OPERATOR_IS_NULL") { return ExpressionType::OPERATOR_IS_NULL; }
  if (str == "OPERATOR_EXISTS") { return ExpressionType::OPERATOR_EXISTS; }
  if (str == "COMPARE_EQUAL" || str == "=") { return ExpressionType::COMPARE_EQUAL; }
  if (str == "COMPARE_NOTEQUAL" || str == "!=" || str == "<>") {
    return ExpressionType::COMPARE_NOT_EQUAL;
  }
  if (str == "COMPARE_LESSTHAN" || str == "<") { return ExpressionType::COMPARE_LESS_THAN; }
  if (str == "COMPARE_GREATERTHAN" || str == ">") { return ExpressionType::COMPARE_GREATER_THAN; }
  if (str == "COMPARE_LESSTHANOREQUALTO" || str == "<=") {
    return ExpressionType::COMPARE_LESS_THAN_OR_EQUAL_TO;
  }
  if (str == "COMPARE_GREATERTHANOREQUALTO" || str == ">=") {
    return ExpressionType::COMPARE_GREATER_THAN_OR_EQUAL_TO;
  }
  if (str == "COMPARE_LIKE" || str == "~~") { return ExpressionType::COMPARE_LIKE; }
  if (str == "COMPARE_NOTLIKE" || str == "!~~") { return ExpressionType::COMPARE_NOT_LIKE; }
  if (str == "COMPARE_IN") { return ExpressionType::COMPARE_IN; }
  if (str == "COMPARE_DISTINCT_FROM") { return ExpressionType::COMPARE_IS_DISTINCT_FROM; }
  if (str == "CONJUNCTION_AND") { return ExpressionType::CONJUNCTION_AND; }
  if (str == "CONJUNCTION_OR") { return ExpressionType::CONJUNCTION_OR; }
  if (str == "COLUMN_VALUE") { return ExpressionType::COLUMN_VALUE; }
  if (str == "VALUE_CONSTANT") { return ExpressionType::VALUE_CONSTANT; }
  if (str == "VALUE_PARAMETER") { return ExpressionType::VALUE_PARAMETER; }
  if (str == "VALUE_TUPLE") { return ExpressionType::VALUE_TUPLE; }
  if (str == "VALUE_TUPLE_ADDRESS") { return ExpressionType::VALUE_TUPLE_ADDRESS; }
  if (str == "VALUE_NULL") { return ExpressionType::VALUE_NULL; }
  if (str == "VALUE_VECTOR") { return ExpressionType::VALUE_VECTOR; }
  if (str == "VALUE_SCALAR") { return ExpressionType::VALUE_SCALAR; }
  if (str == "AGGREGATE_COUNT") { return ExpressionType::AGGREGATE_COUNT; }
  if (str == "AGGREGATE_SUM") { return ExpressionType::AGGREGATE_SUM; }
  if (str == "AGGREGATE_MIN") { return ExpressionType::AGGREGATE_MIN; }
  if (str == "AGGREGATE_MAX") { return ExpressionType::AGGREGATE_MAX; }
  if (str == "AGGREGATE_AVG") { return ExpressionType::AGGREGATE_AVG; }
  if (str == "AGGREGATE_TOP_K") { return ExpressionType::AGGREGATE_TOP_K; }
  if (str == "AGGREGATE_HISTOGRAM") { return ExpressionType::AGGREGATE_HISTOGRAM; }
  if (str == "FUNCTION") { return ExpressionType::FUNCTION; }
  if (str == "HASH_RANGE") { return ExpressionType::HASH_RANGE; }
  if (str == "OPERATOR_CASE_EXPR") { return ExpressionType::OPERATOR_CASE_EXPR; }
  if (str == "OPERATOR_NULLIF") { return ExpressionType::OPERATOR_NULL_IF; }
  if (str == "OPERATOR_COALESCE") { return ExpressionType::OPERATOR_COALESCE; }
  if (str == "ROW_SUBQUERY") { return ExpressionType::ROW_SUBQUERY; }
  if (str == "STAR") { return ExpressionType::STAR; }
  if (str == "TABLE_STAR") { return ExpressionType::TABLE_STAR; }
  if (str == "PLACEHOLDER") { return ExpressionType::PLACEHOLDER; }
  if (str == "COLUMN_REF") { return ExpressionType::COLUMN_REF; }
  if (str == "FUNCTION_REF") { return ExpressionType::FUNCTION_REF; }
  if (str == "TABLE_REF") { return ExpressionType::TABLE_REF; }

  fmt::print("StringToExpressionType: type {} unsupported", str.c_str());
  throw PARSER_EXCEPTION("StringToExpressionType: unsupported type");
}

std::unique_ptr<AbstractExpression> PostgresParser::AExprTransform(ParseResult *parse_result,
                                                                   A_Expr *root) {
  if (root == nullptr) { return nullptr; }

  ExpressionType target_type;
  std::vector<std::unique_ptr<AbstractExpression>> children;

  if (root->kind == AEXPR_DISTINCT) {
    target_type = ExpressionType::COMPARE_IS_DISTINCT_FROM;
    children.emplace_back(ExprTransform(parse_result, root->lexpr, nullptr));
    children.emplace_back(ExprTransform(parse_result, root->rexpr, nullptr));
  } else if (root->kind == AEXPR_IN) {
    // Expression "FOO in (X, Y, ..., Z)". By convention, FOO is the first child
    // and the other children are the IN list.
    target_type = ExpressionType::COMPARE_IN;
    children.emplace_back(ExprTransform(parse_result, root->lexpr, nullptr));
    auto *in_list = reinterpret_cast<List *>(root->rexpr);
    for (int i = 0; i < list_length(in_list); i++) {
      auto *node = static_cast<Node *>(list_nth(in_list, i));
      children.emplace_back(ExprTransform(parse_result, node, nullptr));
    }

    const char *name = strVal(llast(root->name));
    // Postgres distinguishes between IN "=" and NOT IN "<>" by the name of the
    // expression. Rewrite NOT IN.
    if (std::strcmp(name, "<>") == 0) {
      auto in_expr = std::make_unique<ComparisonExpression>(target_type, std::move(children));
      std::vector<std::unique_ptr<AbstractExpression>> in_child;
      in_child.emplace_back(std::move(in_expr));
      return std::make_unique<OperatorExpression>(ExpressionType::OPERATOR_NOT, type_id::INVALID,
                                                  std::move(in_child));
    }
  } else {
    const char *name = strVal(llast(root->name));
    target_type = StringToExpressionType(name);
    children.emplace_back(ExprTransform(parse_result, root->lexpr, nullptr));
    children.emplace_back(ExprTransform(parse_result, root->rexpr, nullptr));
  }

  switch (target_type) {
  case ExpressionType::OPERATOR_UNARY_MINUS:
  case ExpressionType::OPERATOR_PLUS:
  case ExpressionType::OPERATOR_MINUS:
  case ExpressionType::OPERATOR_MULTIPLY:
  case ExpressionType::OPERATOR_DIVIDE:
  case ExpressionType::OPERATOR_CONCAT:
  case ExpressionType::OPERATOR_MOD:
  case ExpressionType::OPERATOR_NOT:
  case ExpressionType::OPERATOR_IS_NULL:
  case ExpressionType::OPERATOR_IS_NOT_NULL:
  case ExpressionType::OPERATOR_EXISTS: {
    return std::make_unique<OperatorExpression>(target_type, type_id::INVALID, std::move(children));
  }
  case ExpressionType::OPERATOR_CAST: {
    return TypeCastTransform(parse_result, reinterpret_cast<TypeCast *>(root));
  }
  case ExpressionType::COMPARE_EQUAL:
  case ExpressionType::COMPARE_NOT_EQUAL:
  case ExpressionType::COMPARE_LESS_THAN:
  case ExpressionType::COMPARE_GREATER_THAN:
  case ExpressionType::COMPARE_LESS_THAN_OR_EQUAL_TO:
  case ExpressionType::COMPARE_GREATER_THAN_OR_EQUAL_TO:
  case ExpressionType::COMPARE_LIKE:
  case ExpressionType::COMPARE_NOT_LIKE:
  case ExpressionType::COMPARE_IN:
  case ExpressionType::COMPARE_IS_DISTINCT_FROM: {
    return std::make_unique<ComparisonExpression>(target_type, std::move(children));
  }
  default: {
    fmt::print("AExprTransform: type {} unsupported", static_cast<int>(target_type));
    throw PARSER_EXCEPTION("AExprTransform: unsupported type");
  }
  }
}

// Postgres.BoolExpr -> noisepage.ConjunctionExpression
std::unique_ptr<AbstractExpression> PostgresParser::BoolExprTransform(ParseResult *parse_result,
                                                                      BoolExpr *root) {
  std::unique_ptr<AbstractExpression> result;
  std::vector<std::unique_ptr<AbstractExpression>> children;
  for (int i = 0; i < list_length(root->args); i++) {
    auto *node = static_cast<Node *>(list_nth(root->args, i));
    children.emplace_back(ExprTransform(parse_result, node, nullptr));
  }
  switch (root->boolop) {
  case AND_EXPR: {
    result = std::make_unique<ConjunctionExpression>(ExpressionType::CONJUNCTION_AND,
                                                     std::move(children));
    break;
  }
  case OR_EXPR: {
    result = std::make_unique<ConjunctionExpression>(ExpressionType::CONJUNCTION_OR,
                                                     std::move(children));
    break;
  }
  case NOT_EXPR: {
    result = std::make_unique<OperatorExpression>(ExpressionType::OPERATOR_NOT, type_id::INVALID,
                                                  std::move(children));
    break;
  }
  default: {
    fmt::print("BoolExprTransform: type {} unsupported\n", static_cast<int>(root->boolop));
    throw PARSER_EXCEPTION("BoolExprTransform: unsupported type");
  }
  }

  return result;
}

std::unique_ptr<AbstractExpression> PostgresParser::CaseExprTransform(ParseResult *parse_result,
                                                                      CaseExpr *root) {
  if (root == nullptr) { return nullptr; }

  auto arg_expr = ExprTransform(parse_result, reinterpret_cast<Node *>(root->arg), nullptr);

  std::vector<CaseExpression::WhenClause> clauses;
  for (int i = 0; i < list_length(root->args); i++) {
    auto *w = static_cast<CaseWhen *>(list_nth(root->args, i));
    auto when_expr = ExprTransform(parse_result, reinterpret_cast<Node *>(w->expr), nullptr);
    auto result_expr = ExprTransform(parse_result, reinterpret_cast<Node *>(w->result), nullptr);

    if (arg_expr == nullptr) {
      auto when_clause = CaseExpression::WhenClause{std::move(when_expr), std::move(result_expr)};
      clauses.emplace_back(std::move(when_clause));
    } else {
      std::vector<std::unique_ptr<AbstractExpression>> children;
      children.emplace_back(arg_expr->Copy());
      children.emplace_back(std::move(when_expr));
      auto cmp_expr = std::make_unique<ComparisonExpression>(ExpressionType::COMPARE_EQUAL,
                                                             std::move(children));
      auto when_clause = CaseExpression::WhenClause{std::move(cmp_expr), std::move(result_expr)};
      clauses.emplace_back(std::move(when_clause));
    }
  }

  auto default_expr =
      ExprTransform(parse_result, reinterpret_cast<Node *>(root->defresult), nullptr);
  auto ret_val_type = clauses[0].then_->GetReturnValueType();
  return std::make_unique<CaseExpression>(ret_val_type, std::move(clauses),
                                          std::move(default_expr));
}

// Postgres.ColumnRef -> noisepage.ColumnValueExpression |
// noisepage.TableStarExpression
std::unique_ptr<AbstractExpression>
PostgresParser::ColumnRefTransform(ParseResult *parse_result, ColumnRef *root, char *alias) {
  std::unique_ptr<AbstractExpression> result;
  List *fields = root->fields;
  auto *node = static_cast<Node *>(linitial(fields));
  switch (nodeTag(node)) {
  case T_String: {
    // TODO(WAN): verify the old system is doing the right thing
    std::string col_name;
    std::string table_name;
    bool all_columns = false;
    if (list_length(fields) == 1) {
      col_name = strVal(node);
      table_name = "";
    } else {
      auto *next_node = static_cast<Node *>(lsecond(fields));
      if (nodeTag(next_node) == T_A_Star) {
        all_columns = true;
      } else {
        col_name = strVal(next_node);
      }

      table_name = strVal(node);
    }

    if (all_columns)
      result = std::make_unique<TableStarExpression>(table_name);
    else if (alias != nullptr)
      /*
       * We create a table alias using the table name. For SELECT queries, the
       * binder will assign the corresponding TableRef a unique serial number.
       * After that, in the binder, we'll have to update this AliasType to have
       * a matching serial number
       */
      result = std::make_unique<ColumnValueExpression>(
          AliasType(table_name), col_name,
          parser::AliasType(
              alias, alias_oid_t(reinterpret_cast<size_t>(reinterpret_cast<void *>(alias)))));
    else
      /*
       * We create a table alias using the table name. For SELECT queries, the
       * binder will assign the corresponding TableRef a unique serial number.
       * After that, in the binder, we'll have to update this AliasType to have
       * a matching serial number
       */
      result = std::make_unique<ColumnValueExpression>(AliasType(table_name), col_name);
    break;
  }
  case T_A_Star: {
    result = std::make_unique<TableStarExpression>();
    break;
  }
  default: {
    fmt::print("ColumnRefTransform: type {} unsupported\n", static_cast<int>(nodeTag(node)));
    throw PARSER_EXCEPTION("ColumnRefTransform: unsupported type");
  }
  }

  return result;
}

// Postgres.A_Const -> noisepage.ConstantValueExpression
std::unique_ptr<AbstractExpression> PostgresParser::ConstTransform(ParseResult *parse_result,
                                                                   A_Const *root) {
  if (root == nullptr) { return nullptr; }
  return ValueTransform(parse_result, root);
}

// Postgres.FuncCall -> noisepage.AbstractExpression
std::unique_ptr<AbstractExpression> PostgresParser::FuncCallTransform(ParseResult *parse_result,
                                                                      FuncCall *root) {
  // TODO(WAN): Check if we need to change the case of this.
  std::string func_name = strVal(linitial(root->funcname));

  std::unique_ptr<AbstractExpression> result;
  if (!IsAggregateFunction(func_name)) {
    // normal functions (built-in functions or UDFs)
    func_name = strVal(llast(root->funcname));
    std::vector<std::unique_ptr<AbstractExpression>> children;

    for (int i = 0; i < list_length(root->args); i++) {
      auto *expr_node = static_cast<Node *>(list_nth(root->args, i));
      children.emplace_back(ExprTransform(parse_result, expr_node, nullptr));
    }
    result = std::make_unique<FunctionExpression>(std::move(func_name), type_id::INVALID,
                                                  std::move(children));
  } else {
    // aggregate function
    auto agg_fun_type = StringToExpressionType("AGGREGATE_" + func_name);
    std::vector<std::unique_ptr<AbstractExpression>> children;
    if (root->agg_star) {
      children.emplace_back(std::make_unique<StarExpression>());
      result = std::make_unique<AggregateExpression>(agg_fun_type, std::move(children),
                                                     root->agg_distinct);
    } else if (list_length(root->args) == 1) {
      auto *expr_node = static_cast<Node *>(linitial(root->args));
      children.emplace_back(ExprTransform(parse_result, expr_node, nullptr));
      result = std::make_unique<AggregateExpression>(agg_fun_type, std::move(children),
                                                     root->agg_distinct);
    } else {
      fmt::print("FuncCallTransform: aggregate must have exactly one argument\n");
      throw PARSER_EXCEPTION("FuncCallTransform: aggregate must have exactly one argument");
    }
  }
  return result;
}

// Postgres.NullTest -> noisepage.OperatorExpression
std::unique_ptr<AbstractExpression> PostgresParser::NullTestTransform(ParseResult *parse_result,
                                                                      NullTest *root) {
  if (root == nullptr) { return nullptr; }

  std::vector<std::unique_ptr<AbstractExpression>> children;
  children.emplace_back(ExprTransform(parse_result, reinterpret_cast<Node *>(root->arg), nullptr));

  ExpressionType type = root->nulltesttype == IS_NULL ? ExpressionType::OPERATOR_IS_NULL
                                                      : ExpressionType::OPERATOR_IS_NOT_NULL;

  return std::make_unique<OperatorExpression>(type, type_id::BOOLEAN, std::move(children));
}

// Postgres.ParamRef -> noisepage.ParameterValueExpression
std::unique_ptr<AbstractExpression> PostgresParser::ParamRefTransform(ParseResult *parse_result,
                                                                      ParamRef *root) {
  return std::make_unique<ParameterValueExpression>(root->number - 1);
}

// Postgres.SubLink -> noisepage.SubqueryExpression / ComparisonExpression /
// OperatorExpression
std::unique_ptr<AbstractExpression> PostgresParser::SubqueryExprTransform(ParseResult *parse_result,
                                                                          SubLink *node) {
  if (node == nullptr) { return nullptr; }

  auto select_stmt = SelectTransform(parse_result, reinterpret_cast<SelectStmt *>(node->subselect));
  auto subquery_expr = std::make_unique<SubqueryExpression>(std::move(select_stmt));
  std::vector<std::unique_ptr<AbstractExpression>> children;
  std::unique_ptr<AbstractExpression> result;

  switch (node->subLinkType) {
  case ANY_SUBLINK: {
    // "x IN (SELECT ...)" has operName == NIL.
    // "x = ANY (SELECT ...)" has operName == ["="], which means the same thing.
    // Anything else, like "x > ANY (SELECT ...)", isn't an IN.
    if (node->operName != nullptr && std::strcmp(strVal(llast(node->operName)), "=") != 0) {
      throw PARSER_EXCEPTION("SubqueryExprTransform: only IN and = ANY are supported");
    }
    auto col_expr = ExprTransform(parse_result, node->testexpr, nullptr);
    children.emplace_back(std::move(col_expr));
    children.emplace_back(std::move(subquery_expr));
    result =
        std::make_unique<ComparisonExpression>(ExpressionType::COMPARE_IN, std::move(children));
    break;
  }
  case EXISTS_SUBLINK: {
    children.emplace_back(std::move(subquery_expr));
    result = std::make_unique<OperatorExpression>(ExpressionType::OPERATOR_EXISTS, type_id::BOOLEAN,
                                                  std::move(children));
    break;
  }
  case EXPR_SUBLINK: {
    result = std::move(subquery_expr);
    break;
  }
  default: {
    fmt::print("SubqueryExprTransform: sublink type {} unsupported\n",
               static_cast<int>(node->subLinkType));
    throw PARSER_EXCEPTION("SubqueryExprTransform: unsupported sublink type");
  }
  }

  return result;
}

// Postgres.TypeCast -> noisepage.TypeCastExpression
std::unique_ptr<AbstractExpression> PostgresParser::TypeCastTransform(ParseResult *parse_result,
                                                                      TypeCast *root) {
  char *type_name = strVal(llast(root->typeName->names));
  auto type = ColumnDefinition::StrToValueType(type_name);
  std::vector<std::unique_ptr<AbstractExpression>> children;
  children.emplace_back(ExprTransform(parse_result, root->arg, nullptr));
  return std::make_unique<TypeCastExpression>(type, std::move(children));
}

// Postgres.A_Const -> noisepage.ConstantValueExpression
std::unique_ptr<AbstractExpression> PostgresParser::ValueTransform(ParseResult *parse_result,
                                                                   A_Const *root) {
  // NULL literals no longer have their own node type
  if (root->isnull) {
    return std::make_unique<ConstantValueExpression>(type_id::INVALID, Val(true));
  }

  switch (nodeTag(&root->val)) {
  case T_Integer: {
    return std::make_unique<ConstantValueExpression>(type_id::INTEGER,
                                                     Integer(PgIntVal(&root->val)));
  }

  case T_String: {
    auto string_val = ValueUtil::CreateStringVal(std::string_view{strVal(&root->val)});
    return std::make_unique<ConstantValueExpression>(type_id::VARCHAR, string_val.first,
                                                     std::move(string_val.second));
  }

  case T_Float: {
    // Per Postgres, T_Float just means that the string looks like a number.
    // T_Float is also used for oversized ints, e.g. BIGINT.
    const char *str = root->val.fval.fval;
    if (std::strpbrk(str, ".eE") == nullptr) {
      return std::make_unique<ConstantValueExpression>(type_id::BIGINT, Integer(std::stoll(str)));
    }
    return std::make_unique<ConstantValueExpression>(type_id::DOUBLE, Real(std::stod(str)));
  }

  default: {
    fmt::print("ValueTransform: value type {} unsupported\n",
               static_cast<int>(nodeTag(&root->val)));
    throw PARSER_EXCEPTION("ValueTransform: unsupported value type");
  }
  }
}

// Reads an integer constant from LIMIT/OFFSET, or returns fallback
// for a missing clause or LIMIT ALL / LIMIT NULL.
static int64_t LimitValue(Node *node, int64_t fallback) {
  if (node == nullptr) { return fallback; }
  if (nodeTag(node) != T_A_Const) {
    throw PARSER_EXCEPTION("LIMIT/OFFSET must be an integer constant");
  }
  auto *c = reinterpret_cast<A_Const *>(node);
  if (c->isnull) { return fallback; }
  switch (nodeTag(&c->val)) {
  case T_Integer: return PgIntVal(&c->val);
  case T_Float: // integers too large for int32 arrive as T_Float
    return std::stoll(c->val.fval.fval);
  default: throw PARSER_EXCEPTION("LIMIT/OFFSET must be an integer constant");
  }
}

std::unique_ptr<SelectStatement> PostgresParser::SelectTransform(ParseResult *parse_result,
                                                                 SelectStmt *root) {
  std::unique_ptr<SelectStatement> result{};

  switch (root->op) {
  case SETOP_NONE: {
    auto target = TargetTransform(parse_result, root->targetList);
    auto from = FromTransform(parse_result, root);
    auto select_distinct = root->distinctClause != nullptr;
    auto groupby = GroupByTransform(parse_result, root->groupClause, root->havingClause);
    auto orderby = OrderByTransform(parse_result, root->sortClause);
    auto where = WhereTransform(parse_result, root->whereClause);
    auto with = WithTransform(parse_result, root->withClause);

    int64_t limit = LimitValue(root->limitCount, LimitDescription::NO_LIMIT);
    int64_t offset = LimitValue(root->limitOffset, LimitDescription::NO_OFFSET);
    auto limit_desc = std::make_unique<LimitDescription>(limit, offset);

    result = std::make_unique<SelectStatement>(std::move(target), select_distinct, std::move(from),
                                               where, std::move(groupby), std::move(orderby),
                                               std::move(limit_desc), std::move(with));
    break;
  }
  case SETOP_UNION: {
    result = SelectTransform(parse_result, root->larg);
    result->SetUnionSelect(SelectTransform(parse_result, root->rarg));
    break;
  }
  default: {
    fmt::print("SelectTransform: set operation {} unsupported\n", static_cast<int>(root->op));
    throw PARSER_EXCEPTION("SelectTransform: unsupported set operation");
  }
  }

  return result;
}

// Postgres.SelectStmt.targetList -> noisepage.SelectStatement.select_
std::vector<shared::ManagedPointer<AbstractExpression>>
PostgresParser::TargetTransform(ParseResult *parse_result, List *root) {
  // Postgres parses 'SELECT;' to nullptr
  if (root == nullptr) { throw PARSER_EXCEPTION("TargetTransform: root==null."); }

  std::vector<shared::ManagedPointer<AbstractExpression>> result{};
  for (int i = 0; i < list_length(root); i++) {
    auto *target = static_cast<ResTarget *>(list_nth(root, i));
    auto expr = ExprTransform(parse_result, target->val, target->name);
    auto expr_managed = shared::ManagedPointer(expr);
    parse_result->AddExpression(std::move(expr));
    result.emplace_back(expr_managed);
  }
  return result;
}

// One item of a FROM clause, or one side of a JOIN
std::unique_ptr<TableRef> PostgresParser::FromItemTransform(ParseResult *parse_result, Node *node) {
  switch (nodeTag(node)) {
  case T_RangeVar: return RangeVarTransform(parse_result, reinterpret_cast<RangeVar *>(node));
  case T_RangeSubselect:
    return RangeSubselectTransform(parse_result, reinterpret_cast<RangeSubselect *>(node));
  case T_JoinExpr:
    return TableRef::CreateTableRefByJoin(
        JoinTransform(parse_result, reinterpret_cast<JoinExpr *>(node)));
  default: ThrowUnsupported("FromItemTransform", "FROM item type", static_cast<int>(nodeTag(node)));
  }
}

// Postgres.SelectStmt.fromClause -> noisepage.TableRef
std::unique_ptr<TableRef> PostgresParser::FromTransform(ParseResult *parse_result,
                                                        SelectStmt *select_root) {
  List *root = select_root->fromClause;

  // Postgres parses 'SELECT 1' (no FROM) to nullptr
  if (root == nullptr) { return nullptr; }

  if (list_length(root) == 1) {
    return FromItemTransform(parse_result, static_cast<Node *>(linitial(root)));
  }

  // FROM a, b, c
  std::vector<std::unique_ptr<TableRef>> refs{};
  for (int i = 0; i < list_length(root); i++) {
    refs.emplace_back(FromItemTransform(parse_result, static_cast<Node *>(list_nth(root, i))));
  }
  return TableRef::CreateTableRefByList(std::move(refs));
}

// Postgres.SelectStmt.groupClause -> noisepage.GroupByDescription
std::unique_ptr<GroupByDescription>
PostgresParser::GroupByTransform(ParseResult *parse_result, List *group, Node *having_node) {
  if (group == nullptr && having_node == nullptr) { return nullptr; }

  std::vector<shared::ManagedPointer<AbstractExpression>> columns;
  for (int i = 0; i < list_length(group); i++) {
    auto *temp = static_cast<Node *>(list_nth(group, i));
    auto expr = ExprTransform(parse_result, temp, nullptr);
    auto expr_ptr = shared::ManagedPointer(expr);
    parse_result->AddExpression(std::move(expr));
    columns.emplace_back(expr_ptr);
  }

  auto having = shared::ManagedPointer<AbstractExpression>(nullptr);
  if (having_node != nullptr) {
    auto expr = ExprTransform(parse_result, having_node, nullptr);
    having = shared::ManagedPointer(expr);
    parse_result->AddExpression(std::move(expr));
  }

  return std::make_unique<GroupByDescription>(std::move(columns), having);
}
std::unique_ptr<OrderByDescription> PostgresParser::OrderByTransform(ParseResult *parse_result,
                                                                     List *order) {
  if (order == nullptr) { return nullptr; }

  std::vector<OrderType> types;
  std::vector<shared::ManagedPointer<AbstractExpression>> exprs;

  for (int i = 0; i < list_length(order); i++) {
    auto *temp = static_cast<Node *>(list_nth(order, i));
    if (nodeTag(temp) != T_SortBy) {
      ThrowUnsupported("OrderByTransform", "ORDER BY item type", static_cast<int>(nodeTag(temp)));
    }
    auto *sort = reinterpret_cast<SortBy *>(temp);

    switch (sort->sortby_dir) {
    case SORTBY_DESC: types.emplace_back(kOrderDesc); break;
    case SORTBY_ASC:
    case SORTBY_DEFAULT: types.emplace_back(kOrderAsc); break;
    default: // SORTBY_USING
      ThrowUnsupported("OrderByTransform", "sort direction", static_cast<int>(sort->sortby_dir));
    }

    auto expr = ExprTransform(parse_result, sort->node, nullptr);
    auto expr_ptr = shared::ManagedPointer(expr);
    parse_result->AddExpression(std::move(expr));
    exprs.emplace_back(expr_ptr);
  }

  return std::make_unique<OrderByDescription>(std::move(types), std::move(exprs));
}

// Postgres.SelectStmt.whereClause -> noisepage.AbstractExpression
shared::ManagedPointer<AbstractExpression> PostgresParser::WhereTransform(ParseResult *parse_result,
                                                                          Node *root) {
  if (root == nullptr) { return nullptr; }
  auto expr = ExprTransform(parse_result, root, nullptr);
  auto result = shared::ManagedPointer(expr);
  parse_result->AddExpression(std::move(expr));
  return result;
}

// Postgres.JoinExpr -> noisepage.JoinDefinition
std::unique_ptr<JoinDefinition> PostgresParser::JoinTransform(ParseResult *parse_result,
                                                              JoinExpr *root) {
  if (root->isNatural) { throw PARSER_EXCEPTION("JoinTransform: NATURAL JOIN is not supported"); }

  JoinType type; // db7's JoinType; root->jointype is Postgres's
  switch (root->jointype) {
  case JOIN_INNER: type = JoinType::INNER; break;
  case JOIN_LEFT: type = JoinType::LEFT; break;
  case JOIN_FULL: type = JoinType::OUTER; break;
  case JOIN_RIGHT: type = JoinType::RIGHT; break;
  case JOIN_SEMI: type = JoinType::SEMI; break;
  default: ThrowUnsupported("JoinTransform", "join type", static_cast<int>(root->jointype));
  }

  auto left = FromItemTransform(parse_result, root->larg);
  auto right = FromItemTransform(parse_result, root->rarg);

  // CROSS JOIN and JOIN ... USING (...) have no ON condition
  if (root->quals == nullptr) {
    throw PARSER_EXCEPTION("JoinTransform: CROSS JOIN and JOIN ... USING are not supported");
  }

  auto expr = ExprTransform(parse_result, root->quals, nullptr);
  auto condition = shared::ManagedPointer(expr);
  parse_result->AddExpression(std::move(expr));

  return std::make_unique<JoinDefinition>(type, std::move(left), std::move(right), condition);
}

AliasType PostgresParser::AliasTransform(Alias *root) {
  if (root == nullptr) { return AliasType(""); }
  return AliasType(root->aliasname);
}

// Postgres.RangeVar -> noisepage.TableRef
std::unique_ptr<TableRef> PostgresParser::RangeVarTransform(ParseResult *parse_result,
                                                            RangeVar *root) {
  auto table_name = root->relname == nullptr ? "" : root->relname;
  auto schema_name = root->schemaname == nullptr ? "" : root->schemaname;
  auto database_name = root->catalogname == nullptr ? "" : root->catalogname;

  auto table_info = std::make_unique<TableInfo>(table_name, schema_name, database_name);
  auto alias = AliasTransform(root->alias);
  auto result = TableRef::CreateTableRefByName(alias, std::move(table_info));
  return result;
}

// Postgres.RangeSubselect -> noisepage.TableRef
std::unique_ptr<TableRef> PostgresParser::RangeSubselectTransform(ParseResult *parse_result,
                                                                  RangeSubselect *root) {
  auto select = SelectTransform(parse_result, reinterpret_cast<SelectStmt *>(root->subquery));
  if (select == nullptr) { return nullptr; }
  auto alias = AliasTransform(root->alias);
  auto result = TableRef::CreateTableRefBySelect(alias, std::move(select));
  return result;
}

// // Postgres.CopyStmt -> noisepage.CopyStatement
// std::unique_ptr<CopyStatement>
// PostgresParser::CopyTransform(ParseResult *parse_result, CopyStmt *root) {
//   static constexpr char k_delimiter_tok[] = "delimiter";
//   static constexpr char k_format_tok[] = "format";
//   static constexpr char k_quote_tok[] = "quote";
//   static constexpr char k_escape_tok[] = "escape";

//   std::unique_ptr<TableRef> table;
//   std::unique_ptr<SelectStatement> select_stmt;
//   if (root->relation != nullptr) {
//     table = RangeVarTransform(parse_result, root->relation);
//   } else {
//     select_stmt = SelectTransform(parse_result,
//                                   reinterpret_cast<SelectStmt
//                                   *>(root->query));
//   }

//   auto file_path = root->filename != nullptr ? root->filename : "";
//   auto is_from = root->is_from;

//   char delimiter = ',';
//   ExternalFileFormat format = ExternalFileFormat::CSV;
//   char quote = '"';
//   char escape = '"';
//   if (root->options != nullptr) {
//     for (ListCell *cell = root->options_->head; cell != nullptr;
//          cell = cell->next) {
//       auto def_elem = reinterpret_cast<DefElem *>(cell->data.ptr_value);

//       if (strncmp(def_elem->defname_, k_format_tok, sizeof(k_format_tok)) ==
//           0) {
//         auto format_cstr = reinterpret_cast<value
//         *>(def_elem->arg_)->val_.str_;
//         // lowercase
//         if (strcmp(format_cstr, "csv") == 0) {
//           format = ExternalFileFormat::CSV;
//         } else if (strcmp(format_cstr, "binary") == 0) {
//           format = ExternalFileFormat::BINARY;
//         }
//       }

//       if (strncmp(def_elem->defname_, k_delimiter_tok,
//                   sizeof(k_delimiter_tok)) == 0) {
//         delimiter = *(reinterpret_cast<value *>(def_elem->arg_)->val_.str_);
//       }

//       if (strncmp(def_elem->defname_, k_quote_tok, sizeof(k_quote_tok)) == 0)
//       {
//         quote = *(reinterpret_cast<value *>(def_elem->arg_)->val_.str_);
//       }

//       if (strncmp(def_elem->defname_, k_escape_tok, sizeof(k_escape_tok)) ==
//           0) {
//         escape = *(reinterpret_cast<value *>(def_elem->arg_)->val_.str_);
//       }
//     }
//   }

//   auto result = std::make_unique<CopyStatement>(
//       std::move(table), std::move(select_stmt), file_path, format, is_from,
//       delimiter, quote, escape);
//   return result;
// }
// Postgres uses NULL for "not given"; our constructors want "".
static const char *OrEmpty(const char *s) { return s != nullptr ? s : ""; }

// RangeVar -> TableInfo (table, schema, database)
static std::unique_ptr<TableInfo> MakeTableInfo(const RangeVar *rv) {
  return std::make_unique<TableInfo>(OrEmpty(rv->relname), OrEmpty(rv->schemaname),
                                     OrEmpty(rv->catalogname));
}

// List of String nodes -> vector of std::string. A null list gives {}.
static std::vector<std::string> StringList(const List *list) {
  std::vector<std::string> out;
  for (int i = 0; i < list_length(list); i++) { out.emplace_back(PgStrVal(list_nth(list, i))); }
  return out;
}

// The string value of an option like FORMAT csv or DELIMITER ','.
static const char *DefElemString(const DefElem *def) {
  if (def->arg == nullptr || nodeTag(def->arg) != T_String) {
    throw PARSER_EXCEPTION(fmt::format("option \"{}\" expects a string value", def->defname));
  }
  return PgStrVal(def->arg);
}

// Postgres type name (as the grammar produces it: int4, int8, float8, bpchar,
// ...) -> function parameter / return type.
static BaseFunctionParameter::DataType FuncDataTypeFromPgName(const char *name) {
  std::string_view n{name};
  if (n == "int" || n == "int4") return BaseFunctionParameter::DataType::INT;
  if (n == "varchar") return BaseFunctionParameter::DataType::VARCHAR;
  if (n == "int8") return BaseFunctionParameter::DataType::BIGINT;
  if (n == "int2") return BaseFunctionParameter::DataType::SMALLINT;
  if (n == "double" || n == "float8") return BaseFunctionParameter::DataType::DOUBLE;
  if (n == "real" || n == "float4") return BaseFunctionParameter::DataType::FLOAT;
  if (n == "text") return BaseFunctionParameter::DataType::TEXT;
  if (n == "bpchar") return BaseFunctionParameter::DataType::CHAR;
  if (n == "tinyint") return BaseFunctionParameter::DataType::TINYINT;
  if (n == "bool") return BaseFunctionParameter::DataType::BOOL;
  throw PARSER_EXCEPTION(fmt::format("unsupported function data type \"{}\"", name));
}

// //
// ---------------------------------------------------------------------------
// // COPY
// //
// ---------------------------------------------------------------------------

// // Postgres.CopyStmt -> noisepage.CopyStatement
// std::unique_ptr<CopyStatement>
// PostgresParser::CopyTransform(ParseResult *parse_result, CopyStmt *root) {
//   std::unique_ptr<TableRef> table;
//   std::unique_ptr<SelectStatement> select_stmt;
//   if (root->relation != nullptr) {
//     table = RangeVarTransform(parse_result, root->relation);
//   } else {
//     if (root->query == nullptr || nodeTag(root->query) != T_SelectStmt) {
//       throw PARSER_EXCEPTION("CopyTransform: only COPY (SELECT ...) is
//       supported");
//     }
//     select_stmt = SelectTransform(parse_result,
//                                   reinterpret_cast<SelectStmt
//                                   *>(root->query));
//   }

//   const char *file_path = OrEmpty(root->filename);
//   bool is_from = root->is_from;

//   char delimiter = ',';
//   ExternalFileFormat format = ExternalFileFormat::CSV;
//   char quote = '"';
//   char escape = '"';

//   for (int i = 0; i < list_length(root->options); i++) {
//     auto *def_elem = static_cast<DefElem *>(list_nth(root->options, i));

//     if (std::strcmp(def_elem->defname, "format") == 0) {
//       const char *value = DefElemString(def_elem);
//       if (strcasecmp(value, "csv") == 0) {
//         format = ExternalFileFormat::CSV;
//       } else if (strcasecmp(value, "binary") == 0) {
//         format = ExternalFileFormat::BINARY;
//       } else {
//         throw PARSER_EXCEPTION(
//             fmt::format("CopyTransform: unsupported format \"{}\"", value));
//       }
//     } else if (std::strcmp(def_elem->defname, "delimiter") == 0) {
//       delimiter = DefElemString(def_elem)[0];
//     } else if (std::strcmp(def_elem->defname, "quote") == 0) {
//       quote = DefElemString(def_elem)[0];
//     } else if (std::strcmp(def_elem->defname, "escape") == 0) {
//       escape = DefElemString(def_elem)[0];
//     }
//     // Other options (HEADER, NULL, ...) are ignored, as before.
//   }

//   return std::make_unique<CopyStatement>(std::move(table),
//   std::move(select_stmt),
//                                          file_path, format, is_from,
//                                          delimiter, quote, escape);
// }

// ---------------------------------------------------------------------------
// CREATE
// ---------------------------------------------------------------------------

// Postgres.CreateStmt -> noisepage.CreateStatement
std::unique_ptr<SQLStatement> PostgresParser::CreateTransform(ParseResult *parse_result,
                                                              CreateStmt *root) {
  auto table_info = MakeTableInfo(root->relation);

  std::unordered_set<std::string> primary_keys;
  std::vector<std::unique_ptr<ColumnDefinition>> columns;
  std::vector<std::unique_ptr<ColumnDefinition>> foreign_keys;

  for (int i = 0; i < list_length(root->tableElts); i++) {
    auto *node = static_cast<Node *>(list_nth(root->tableElts, i));
    switch (nodeTag(node)) {
    case T_ColumnDef: {
      auto res = ColumnDefTransform(parse_result, reinterpret_cast<ColumnDef *>(node));
      columns.emplace_back(std::move(res.col_));
      foreign_keys.insert(foreign_keys.end(), std::make_move_iterator(res.fks_.begin()),
                          std::make_move_iterator(res.fks_.end()));
      break;
    }
    case T_Constraint: {
      // Table-level constraints, e.g. PRIMARY KEY (a, b)
      auto *constraint = reinterpret_cast<Constraint *>(node);
      switch (constraint->contype) {
      case CONSTR_PRIMARY: {
        for (auto &key : StringList(constraint->keys)) { primary_keys.emplace(std::move(key)); }
        break;
      }
      case CONSTR_FOREIGN: {
        if (constraint->pk_attrs == nullptr) {
          throw NOT_IMPLEMENTED_EXCEPTION(
              "CreateTransform: foreign key must list referenced columns");
        }
        auto fk = std::make_unique<ColumnDefinition>(
            StringList(constraint->fk_attrs), StringList(constraint->pk_attrs),
            constraint->pktable->relname, CharToActionType(constraint->fk_del_action),
            CharToActionType(constraint->fk_upd_action), CharToMatchType(constraint->fk_matchtype));
        foreign_keys.emplace_back(std::move(fk));
        break;
      }
      default: {
        fmt::print("CreateTransform: constraint of type {} not supported\n",
                   static_cast<int>(constraint->contype));
        throw NOT_IMPLEMENTED_EXCEPTION("CreateTransform error");
      }
      }
      break;
    }
    default: {
      fmt::print("CreateTransform: tableElt type {} not supported\n",
                 static_cast<int>(nodeTag(node)));
      throw NOT_IMPLEMENTED_EXCEPTION("CreateTransform error");
    }
    }
  }

  for (auto &column : columns) {
    // skip foreign key constraint
    if (column->GetColumnName().empty()) { continue; }
    if (primary_keys.find(column->GetColumnName()) != primary_keys.end()) {
      column->SetPrimary(true);
    }
  }

  return std::make_unique<CreateStatement>(std::move(table_info),
                                           CreateStatement::CreateType::kTable, std::move(columns),
                                           std::move(foreign_keys));
}

// Postgres.CreatedbStmt -> noisepage.CreateStatement
std::unique_ptr<SQLStatement> PostgresParser::CreateDatabaseTransform(ParseResult *parse_result,
                                                                      CreatedbStmt *root) {
  auto table_info = std::make_unique<TableInfo>("", "", root->dbname);
  std::vector<std::unique_ptr<ColumnDefinition>> columns;
  std::vector<std::unique_ptr<ColumnDefinition>> foreign_keys;
  // TODO(WAN): per the old system, more options need to be converted
  return std::make_unique<CreateStatement>(std::move(table_info), CreateStatement::kDatabase,
                                           std::move(columns), std::move(foreign_keys));
}

// Postgres.CreateFunctionStmt -> noisepage.CreateFunctionStatement
std::unique_ptr<SQLStatement> PostgresParser::CreateFunctionTransform(ParseResult *parse_result,
                                                                      CreateFunctionStmt *root) {
  if (root->is_procedure) { throw PARSER_EXCEPTION("CREATE PROCEDURE is not supported"); }
  if (root->sql_body != nullptr) {
    throw PARSER_EXCEPTION("BEGIN ATOMIC function bodies are not supported");
  }
  if (root->returnType == nullptr) {
    throw PARSER_EXCEPTION("CreateFunctionTransform: RETURNS type is required");
  }

  bool replace = root->replace;

  std::vector<std::unique_ptr<FuncParameter>> func_parameters;
  for (int i = 0; i < list_length(root->parameters); i++) {
    auto *node = static_cast<Node *>(list_nth(root->parameters, i));
    if (nodeTag(node) == T_FunctionParameter) {
      func_parameters.emplace_back(
          FunctionParameterTransform(parse_result, reinterpret_cast<FunctionParameter *>(node)));
    }
    // TODO(WAN): previous code just ignored other node types
  }

  auto return_type = ReturnTypeTransform(parse_result, root->returnType);

  // TODO(WAN): assumption from old code, can only pass one function name
  std::string func_name = PgStrVal(llast(root->funcname));

  std::vector<std::string> func_body;
  AsType as_type = AsType::INVALID;
  PLType pl_type = PLType::INVALID;

  for (int i = 0; i < list_length(root->options); i++) {
    auto *def_elem = static_cast<DefElem *>(list_nth(root->options, i));
    if (std::strcmp(def_elem->defname, "as") == 0) {
      // AS 'body'  or  AS 'obj_file', 'link_symbol'
      func_body = StringList(reinterpret_cast<List *>(def_elem->arg));
      as_type = func_body.size() > 1 ? AsType::EXECUTABLE : AsType::QUERY_STRING;
    } else if (std::strcmp(def_elem->defname, "language") == 0) {
      const char *lang = DefElemString(def_elem);
      if (std::strcmp(lang, "plpgsql") == 0) {
        pl_type = PLType::PL_PGSQL;
      } else if (std::strcmp(lang, "c") == 0) {
        pl_type = PLType::PL_C;
      } else {
        throw PARSER_EXCEPTION(
            fmt::format("CreateFunctionTransform: language \"{}\" not supported", lang));
      }
    }
  }

  return std::make_unique<CreateFunctionStatement>(replace, std::move(func_name),
                                                   std::move(func_body), std::move(return_type),
                                                   std::move(func_parameters), pl_type, as_type);
}

// Postgres.IndexStmt -> noisepage.CreateStatement
std::unique_ptr<SQLStatement> PostgresParser::CreateIndexTransform(ParseResult *parse_result,
                                                                   IndexStmt *root) {
  auto unique = root->unique;

  assert(root->relation->relname != nullptr && "It can't be empty. See postgres spec.");

  const char *table_name = root->relation->relname;
  auto table_info = MakeTableInfo(root->relation);

  const bool no_name = root->idxname == nullptr;
  std::string index_name = no_name ? table_name : root->idxname;

  std::vector<IndexAttr> index_attrs;
  for (int i = 0; i < list_length(root->indexParams); i++) {
    auto *index_elem = static_cast<IndexElem *>(list_nth(root->indexParams, i));
    if (index_elem->expr == nullptr) {
      index_attrs.emplace_back(index_elem->name);
      if (no_name) { index_name += "_" + std::string(index_elem->name); }
    } else {
      auto expr = ExprTransform(parse_result, index_elem->expr, nullptr);
      auto expr_ptr = shared::ManagedPointer(expr);
      parse_result->AddExpression(std::move(expr));
      index_attrs.emplace_back(expr_ptr);
    }
  }

  if (no_name) { index_name += "_idx"; }

  // The grammar fills in "btree" when there is no USING clause.
  const char *access_method = root->accessMethod;
  IndexType index_type;
  if (std::strcmp(access_method, "invalid") == 0) {
    index_type = IndexType::INVALID;
  } else if (std::strcmp(access_method, "bwtree") == 0) {
    index_type = IndexType::BWTREE;
  } else if (std::strcmp(access_method, "btree") == 0 ||
             std::strcmp(access_method, "bplustree") == 0) {
    index_type = IndexType::BPLUSTREE;
  } else if (std::strcmp(access_method, "hash") == 0) {
    index_type = IndexType::HASH;
  } else {
    fmt::print("CreateIndexTransform: IndexType {} not supported\n", access_method);
    throw NOT_IMPLEMENTED_EXCEPTION("CreateIndexTransform error");
  }

  return std::make_unique<CreateStatement>(std::move(table_info), index_type, unique, index_name,
                                           std::move(index_attrs));
}

// Postgres.CreateSchemaStmt -> noisepage.CreateStatement
std::unique_ptr<SQLStatement> PostgresParser::CreateSchemaTransform(ParseResult *parse_result,
                                                                    CreateSchemaStmt *root) {
  std::string schema_name;
  if (root->schemaname != nullptr) {
    schema_name = root->schemaname;
  } else {
    // CREATE SCHEMA AUTHORIZATION role: schema is named after the role
    assert(root->authrole != nullptr && "We need a schema name.");
    if (root->authrole->rolename == nullptr) {
      // AUTHORIZATION CURRENT_USER / SESSION_USER has no literal name
      throw PARSER_EXCEPTION("CreateSchemaTransform: schema name required");
    }
    schema_name = root->authrole->rolename;
  }

  if (root->schemaElts != nullptr) {
    throw PARSER_EXCEPTION("CreateSchemaTransform schema_element unsupported");
  }

  auto table_info = std::make_unique<TableInfo>("", schema_name, "");
  return std::make_unique<CreateStatement>(std::move(table_info), root->if_not_exists);
}

// Postgres.CreateTrigStmt -> noisepage.CreateStatement
std::unique_ptr<SQLStatement> PostgresParser::CreateTriggerTransform(ParseResult *parse_result,
                                                                     CreateTrigStmt *root) {
  auto table_info = MakeTableInfo(root->relation);
  auto trigger_name = root->trigname;

  std::vector<std::string> trigger_funcnames = StringList(root->funcname);
  std::vector<std::string> trigger_args = StringList(root->args);
  std::vector<std::string> trigger_columns = StringList(root->columns);

  auto trigger_when = WhenTransform(parse_result, root->whenClause);
  auto trigger_when_ptr = shared::ManagedPointer(trigger_when);
  if (trigger_when != nullptr) { parse_result->AddExpression(std::move(trigger_when)); }

  // Same bit layout as Postgres's catalog/pg_trigger.h: TRIGGER_TYPE_ROW is
  // bit 0, and root->timing / root->events already use TRIGGER_TYPE_* bits.
  constexpr int16_t kTriggerTypeRow = 1 << 0;
  int16_t trigger_type = 0;
  if (root->row) { trigger_type |= kTriggerTypeRow; }
  trigger_type |= root->timing;
  trigger_type |= root->events;

  return std::make_unique<CreateStatement>(
      std::move(table_info), trigger_name, std::move(trigger_funcnames), std::move(trigger_args),
      std::move(trigger_columns), trigger_when_ptr, trigger_type);
}

// Postgres.ViewStmt -> noisepage.CreateStatement
std::unique_ptr<SQLStatement> PostgresParser::CreateViewTransform(ParseResult *parse_result,
                                                                  ViewStmt *root) {
  auto view_name = root->view->relname;

  if (nodeTag(root->query) != T_SelectStmt) {
    throw PARSER_EXCEPTION("CREATE VIEW as query only supports SELECT");
  }
  auto view_query = SelectTransform(parse_result, reinterpret_cast<SelectStmt *>(root->query));

  return std::make_unique<CreateStatement>(view_name, std::move(view_query));
}

// Postgres.ColumnDef -> noisepage.ColumnDefinition
PostgresParser::ColumnDefTransResult PostgresParser::ColumnDefTransform(ParseResult *parse_result,
                                                                        ColumnDef *root) {
  TypeName *type_name = root->typeName;

  // varchar(32) -> typmods = [A_Const 32]
  int32_t varlen = -1;
  if (type_name->typmods != nullptr) {
    auto *node = static_cast<Node *>(linitial(type_name->typmods));
    if (nodeTag(node) != T_A_Const) {
      ThrowUnsupported("ColumnDefTransform", "type modifier", static_cast<int>(nodeTag(node)));
    }
    auto *c = reinterpret_cast<A_Const *>(node);
    if (c->isnull || nodeTag(&c->val) != T_Integer) {
      ThrowUnsupported("ColumnDefTransform", "type modifier value",
                       static_cast<int>(nodeTag(&c->val)));
    }
    varlen = static_cast<int32_t>(PgIntVal(&c->val));
  }

  const char *datatype_name = PgStrVal(llast(type_name->names));
  auto datatype = ColumnDefinition::StrToDataType(datatype_name);

  std::vector<std::unique_ptr<ColumnDefinition>> foreign_keys;

  bool is_primary = false;
  bool is_not_null = false;
  bool is_unique = false;
  auto default_expr = shared::ManagedPointer<AbstractExpression>(nullptr);
  auto check_expr = shared::ManagedPointer<AbstractExpression>(nullptr);

  for (int i = 0; i < list_length(root->constraints); i++) {
    auto *node = static_cast<Node *>(list_nth(root->constraints, i));
    // COLLATE clauses also show up in this list
    if (nodeTag(node) != T_Constraint) {
      ThrowUnsupported("ColumnDefTransform", "column option", static_cast<int>(nodeTag(node)));
    }
    auto *constraint = reinterpret_cast<Constraint *>(node);

    switch (constraint->contype) {
    case CONSTR_NULL: // explicit NULL, the default
      break;
    case CONSTR_PRIMARY: is_primary = true; break;
    case CONSTR_NOTNULL: is_not_null = true; break;
    case CONSTR_UNIQUE: is_unique = true; break;
    case CONSTR_FOREIGN: {
      if (constraint->pk_attrs == nullptr) {
        throw NOT_IMPLEMENTED_EXCEPTION("Foreign key columns unspecified");
      }
      std::vector<std::string> fk_sinks{PgStrVal(linitial(constraint->pk_attrs))};
      std::vector<std::string> fk_sources{root->colname};

      auto coldef = std::make_unique<ColumnDefinition>(
          std::move(fk_sources), std::move(fk_sinks), constraint->pktable->relname,
          CharToActionType(constraint->fk_del_action), CharToActionType(constraint->fk_upd_action),
          CharToMatchType(constraint->fk_matchtype));
      foreign_keys.emplace_back(std::move(coldef));
      break;
    }
    case CONSTR_DEFAULT: {
      auto expr = ExprTransform(parse_result, constraint->raw_expr, nullptr);
      default_expr = shared::ManagedPointer(expr);
      parse_result->AddExpression(std::move(expr));
      break;
    }
    case CONSTR_CHECK: {
      auto expr = ExprTransform(parse_result, constraint->raw_expr, nullptr);
      check_expr = shared::ManagedPointer(expr);
      parse_result->AddExpression(std::move(expr));
      break;
    }
    default:
      ThrowUnsupported("ColumnDefTransform", "constraint", static_cast<int>(constraint->contype));
    }
  }

  auto result = std::make_unique<ColumnDefinition>(root->colname, datatype, is_primary, is_not_null,
                                                   is_unique, default_expr, check_expr, varlen);

  return {std::move(result), std::move(foreign_keys)};
}

// Postgres.FunctionParameter -> noisepage.FuncParameter
std::unique_ptr<FuncParameter> PostgresParser::FunctionParameterTransform(ParseResult *parse_result,
                                                                          FunctionParameter *root) {
  auto data_type = FuncDataTypeFromPgName(PgStrVal(llast(root->argType->names)));
  const char *param_name = OrEmpty(root->name);
  return std::make_unique<FuncParameter>(data_type, param_name);
}

// Postgres.TypeName -> noisepage.ReturnType
std::unique_ptr<ReturnType> PostgresParser::ReturnTypeTransform(ParseResult *parse_result,
                                                                TypeName *root) {
  auto data_type = FuncDataTypeFromPgName(PgStrVal(llast(root->names)));
  return std::make_unique<ReturnType>(data_type);
}

// Postgres.Node -> noisepage.AbstractExpression (trigger WHEN condition)
std::unique_ptr<AbstractExpression> PostgresParser::WhenTransform(ParseResult *parse_result,
                                                                  Node *root) {
  return ExprTransform(parse_result, root, nullptr); // nullptr stays nullptr
}

// ---------------------------------------------------------------------------
// DELETE / DROP / TRUNCATE
// ---------------------------------------------------------------------------

// Postgres.DeleteStmt -> noisepage.DeleteStatement
// std::unique_ptr<DeleteStatement>
// PostgresParser::DeleteTransform(ParseResult *parse_result, DeleteStmt *root)
// {
//   if (root->usingClause != nullptr) {
//     throw PARSER_EXCEPTION(
//         "DeleteTransform: DELETE ... USING is not supported");
//   }
//   auto table = RangeVarTransform(parse_result, root->relation);
//   auto where = WhereTransform(parse_result, root->whereClause);
//   return std::make_unique<DeleteStatement>(std::move(table), where);
// }

// Postgres.DropStmt -> noisepage.DropStatement
// std::unique_ptr<DropStatement>
// PostgresParser::DropTransform(ParseResult *parse_result, DropStmt *root) {
//   switch (root->removeType) {
//   case OBJECT_INDEX:
//     return DropIndexTransform(parse_result, root);
//   case OBJECT_SCHEMA:
//     return DropSchemaTransform(parse_result, root);
//   case OBJECT_TABLE:
//     return DropTableTransform(parse_result, root);
//   case OBJECT_TRIGGER:
//     return DropTriggerTransform(parse_result, root);
//   default:
//     ThrowUnsupported("DropTransform", "object type",
//                      static_cast<int>(root->removeType));
//   }
// }

// Postgres.DropdbStmt -> noisepage.DropStatement
// std::unique_ptr<DropStatement>
// PostgresParser::DropDatabaseTransform(ParseResult *parse_result,
//                                       DropdbStmt *root) {
//   auto table_info = std::make_unique<TableInfo>("", "", root->dbname);
//   return std::make_unique<DropStatement>(std::move(table_info),
//                                          DropStatement::DropType::kDatabase,
//                                          root->missing_ok);
// }

// DROP INDEX [schema.]name
// std::unique_ptr<DropStatement>
// PostgresParser::DropIndexTransform(ParseResult *parse_result, DropStmt *root)
// {
//   // objects is a list of name lists; we only handle the first one
//   auto *names = static_cast<List *>(linitial(root->objects));

//   std::string schema_name;
//   std::string index_name = PgStrVal(llast(names));
//   if (list_length(names) >= 2) {
//     schema_name = PgStrVal(list_nth(names, list_length(names) - 2));
//   }

//   auto table_info = std::make_unique<TableInfo>("", schema_name, "");
//   return std::make_unique<DropStatement>(std::move(table_info), index_name);
// }

// // DROP SCHEMA name [CASCADE]
// std::unique_ptr<DropStatement>
// PostgresParser::DropSchemaTransform(ParseResult *parse_result, DropStmt
// *root) {
//   auto if_exists = root->missing_ok;
//   auto cascade = root->behavior == DROP_CASCADE;

//   // For schemas, objects is a flat list of String nodes (not a list of
//   lists) std::string schema_name = PgStrVal(linitial(root->objects));

//   auto table_info = std::make_unique<TableInfo>("", schema_name, "");
//   return std::make_unique<DropStatement>(std::move(table_info), if_exists,
//                                          cascade);
// }

// // DROP TABLE [schema.]name
// std::unique_ptr<DropStatement>
// PostgresParser::DropTableTransform(ParseResult *parse_result, DropStmt *root)
// {
//   auto *names = static_cast<List *>(linitial(root->objects));

//   std::string schema_name;
//   std::string table_name = PgStrVal(llast(names));
//   if (list_length(names) >= 2) {
//     schema_name = PgStrVal(list_nth(names, list_length(names) - 2));
//   }

//   auto table_info = std::make_unique<TableInfo>(table_name, schema_name, "");
//   return std::make_unique<DropStatement>(
//       std::move(table_info), DropStatement::DropType::kTable,
//       root->missing_ok);
// }

// // TRUNCATE table -> DELETE without WHERE
// std::unique_ptr<DeleteStatement>
// PostgresParser::TruncateTransform(ParseResult *parse_result,
//                                   TruncateStmt *truncate_stmt) {
//   if (list_length(truncate_stmt->relations) != 1) {
//     throw PARSER_EXCEPTION("TruncateTransform: exactly one table is
//     supported");
//   }
//   auto table_ref = RangeVarTransform(
//       parse_result,
//       static_cast<RangeVar *>(linitial(truncate_stmt->relations)));
//   return std::make_unique<DeleteStatement>(std::move(table_ref));
// }

// // DROP TRIGGER name ON [schema.]table
// std::unique_ptr<DropStatement>
// PostgresParser::DropTriggerTransform(ParseResult *parse_result,
//                                      DropStmt *root) {
//   // objects = [[schema,] table, trigger]
//   auto *names = static_cast<List *>(linitial(root->objects));

//   std::string trigger_name = PgStrVal(llast(names));
//   std::string table_name;
//   std::string schema_name;
//   if (list_length(names) == 3) {
//     schema_name = PgStrVal(linitial(names));
//     table_name = PgStrVal(lsecond(names));
//   } else {
//     table_name = PgStrVal(linitial(names));
//   }

//   auto table_info = std::make_unique<TableInfo>(table_name, schema_name, "");
//   return std::make_unique<DropStatement>(
//       std::move(table_info), DropStatement::DropType::kTrigger,
//       trigger_name);
// }

// //
// ---------------------------------------------------------------------------
// // EXECUTE / EXPLAIN / PREPARE
// //
// ---------------------------------------------------------------------------

// std::unique_ptr<ExecuteStatement>
// PostgresParser::ExecuteTransform(ParseResult *parse_result, ExecuteStmt
// *root) {
//   auto params = ParamListTransform(parse_result, root->params);
//   return std::make_unique<ExecuteStatement>(root->name, std::move(params));
// }

// // List of expressions (EXECUTE parameters, SET values) -> managed
// expressions std::vector<shared::ManagedPointer<AbstractExpression>>
// PostgresParser::ParamListTransform(ParseResult *parse_result, List *root) {
//   std::vector<shared::ManagedPointer<AbstractExpression>> result;
//   for (int i = 0; i < list_length(root); i++) {
//     auto expr = ExprTransform(parse_result,
//                               static_cast<Node *>(list_nth(root, i)),
//                               nullptr);
//     result.emplace_back(shared::ManagedPointer(expr));
//     parse_result->AddExpression(std::move(expr));
//   }
//   return result;
// }

// std::unique_ptr<ExplainStatement>
// PostgresParser::ExplainTransform(ParseResult *parse_result, ExplainStmt
// *root) {
//   auto query = NodeTransform(parse_result, root->query);
//   auto result = std::make_unique<ExplainStatement>(std::move(query));

//   for (int i = 0; i < list_length(root->options); i++) {
//     auto *def_elem = static_cast<DefElem *>(list_nth(root->options, i));
//     auto cursorpos = static_cast<uint32_t>(std::max(def_elem->location, 0));

//     if (std::strcmp(def_elem->defname, "format") != 0) {
//       throw ParserException("Unsupported option string for EXPLAIN.",
//       __FILE__,
//                             __LINE__, cursorpos);
//     }

//     const char *format = DefElemString(def_elem);
//     if (strcasecmp(format, "tpl") == 0) {
//       result->SetFormat(ExplainStatementFormat::TPL);
//     } else if (strcasecmp(format, "tbc") == 0) {
//       result->SetFormat(ExplainStatementFormat::TBC);
//     } else if (strcasecmp(format, "json") == 0) {
//       // this is the default format for us anyway so it's a noop
//       assert(result->GetFormat() == ExplainStatementFormat::JSON &&
//              "We assume this is the default format.");
//     } else {
//       throw ParserException("Unsupported format string for EXPLAIN.",
//       __FILE__,
//                             __LINE__, cursorpos);
//     }
//   }
//   return result;
// }

// std::unique_ptr<PrepareStatement>
// PostgresParser::PrepareTransform(ParseResult *parse_result, PrepareStmt
// *root) {
//   auto query = NodeTransform(parse_result, root->query);

//   // TODO(WAN): This should probably be populated?
//   std::vector<shared::ManagedPointer<ParameterValueExpression>> placeholders;

//   return std::make_unique<PrepareStatement>(root->name, std::move(query),
//                                             std::move(placeholders));
// }

// ---------------------------------------------------------------------------
// INSERT
// ---------------------------------------------------------------------------

// Postgres.InsertStmt -> noisepage.InsertStatement
// std::unique_ptr<InsertStatement>
// PostgresParser::InsertTransform(ParseResult *parse_result, InsertStmt *root)
// {
//   if (root->selectStmt == nullptr) {
//     throw PARSER_EXCEPTION(
//         "InsertTransform: INSERT ... DEFAULT VALUES is not supported");
//   }
//   if (root->onConflictClause != nullptr) {
//     throw PARSER_EXCEPTION("InsertTransform: ON CONFLICT is not supported");
//   }

//   auto column_names = ColumnNameTransform(root->cols);
//   auto table_ref = RangeVarTransform(parse_result, root->relation);
//   auto *select_stmt = reinterpret_cast<SelectStmt *>(root->selectStmt);

//   if (select_stmt->valuesLists != nullptr) {
//     // INSERT ... VALUES (...), (...)
//     auto insert_values =
//         ValueListsTransform(parse_result, select_stmt->valuesLists);
//     return std::make_unique<InsertStatement>(std::move(column_names),
//                                              std::move(table_ref),
//                                              std::move(insert_values));
//   }

//   // INSERT ... SELECT ...
//   auto select_trans = SelectTransform(parse_result, select_stmt);
//   return std::make_unique<InsertStatement>(
//       std::move(column_names), std::move(table_ref),
//       std::move(select_trans));
// }

// INSERT column list (ResTarget nodes) -> column names
std::unique_ptr<std::vector<std::string>> PostgresParser::ColumnNameTransform(List *root) {
  auto result = std::make_unique<std::vector<std::string>>();
  for (int i = 0; i < list_length(root); i++) {
    auto *target = static_cast<ResTarget *>(list_nth(root, i));
    result->emplace_back(target->name);
  }
  return result;
}

// VALUES lists -> one vector of expressions per row
std::unique_ptr<std::vector<std::vector<shared::ManagedPointer<AbstractExpression>>>>
PostgresParser::ValueListsTransform(ParseResult *parse_result, List *root) {
  auto result =
      std::make_unique<std::vector<std::vector<shared::ManagedPointer<AbstractExpression>>>>();

  for (int i = 0; i < list_length(root); i++) {
    auto *row = static_cast<List *>(list_nth(root, i));
    std::vector<shared::ManagedPointer<AbstractExpression>> cur_result;

    for (int j = 0; j < list_length(row); j++) {
      auto *node = static_cast<Node *>(list_nth(row, j));
      std::unique_ptr<AbstractExpression> expr;
      if (nodeTag(node) == T_SetToDefault) {
        expr = std::make_unique<DefaultValueExpression>();
      } else {
        expr = ExprTransform(parse_result, node, nullptr);
      }
      cur_result.emplace_back(shared::ManagedPointer(expr));
      parse_result->AddExpression(std::move(expr));
    }
    result->emplace_back(std::move(cur_result));
  }

  return result;
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

// std::unique_ptr<TransactionStatement>
// PostgresParser::TransactionTransform(TransactionStmt *transaction_stmt) {
//   switch (transaction_stmt->kind) {
//   case TRANS_STMT_BEGIN:
//   case TRANS_STMT_START: // START TRANSACTION == BEGIN
//     return
//     std::make_unique<TransactionStatement>(TransactionStatement::kBegin);
//   case TRANS_STMT_COMMIT:
//     return std::make_unique<TransactionStatement>(
//         TransactionStatement::kCommit);
//   case TRANS_STMT_ROLLBACK:
//     return std::make_unique<TransactionStatement>(
//         TransactionStatement::kRollback);
//   default:
//     ThrowUnsupported("TransactionTransform", "transaction statement type",
//                      static_cast<int>(transaction_stmt->kind));
//   }
// }

// ---------------------------------------------------------------------------
// UPDATE
// ---------------------------------------------------------------------------

// SET col = expr, ... -> UpdateClauses
// std::vector<std::unique_ptr<UpdateClause>>
// PostgresParser::UpdateTargetTransform(ParseResult *parse_result, List *root)
// {
//   std::vector<std::unique_ptr<UpdateClause>> result;
//   for (int i = 0; i < list_length(root); i++) {
//     auto *target = static_cast<ResTarget *>(list_nth(root, i));
//     auto expr = ExprTransform(parse_result, target->val, nullptr);
//     auto expr_ptr = shared::ManagedPointer(expr);
//     parse_result->AddExpression(std::move(expr));
//     result.emplace_back(std::make_unique<UpdateClause>(target->name,
//     expr_ptr));
//   }
//   return result;
// }

// // Postgres.UpdateStmt -> noisepage.UpdateStatement
// std::unique_ptr<UpdateStatement>
// PostgresParser::UpdateTransform(ParseResult *parse_result,
//                                 UpdateStmt *update_stmt) {
//   if (update_stmt->fromClause != nullptr) {
//     throw PARSER_EXCEPTION("UpdateTransform: UPDATE ... FROM is not
//     supported");
//   }
//   auto table = RangeVarTransform(parse_result, update_stmt->relation);
//   auto clauses = UpdateTargetTransform(parse_result,
//   update_stmt->targetList); auto where = WhereTransform(parse_result,
//   update_stmt->whereClause); return
//   std::make_unique<UpdateStatement>(std::move(table), std::move(clauses),
//                                            where);
// }

// ---------------------------------------------------------------------------
// ANALYZE / SET / SHOW
// ---------------------------------------------------------------------------

// ANALYZE [table [(col, ...)]] -> noisepage.AnalyzeStatement
// std::unique_ptr<AnalyzeStatement>
// PostgresParser::VacuumTransform(ParseResult *parse_result, VacuumStmt *root)
// {
//   if (root->is_vacuumcmd) {
//     throw PARSER_EXCEPTION(
//         "VacuumTransform: VACUUM is not supported, only ANALYZE");
//   }
//   if (list_length(root->rels) > 1) {
//     throw PARSER_EXCEPTION(
//         "VacuumTransform: ANALYZE supports one table at a time");
//   }

//   std::unique_ptr<TableRef> analyze_table;
//   auto analyze_columns = std::make_unique<std::vector<std::string>>();
//   if (list_length(root->rels) == 1) {
//     auto *rel = static_cast<VacuumRelation *>(linitial(root->rels));
//     analyze_table = RangeVarTransform(parse_result, rel->relation);
//     *analyze_columns =
//         StringList(rel->va_cols); // column names are String nodes
//   }

//   return std::make_unique<AnalyzeStatement>(std::move(analyze_table),
//                                             std::move(analyze_columns));
// }

// // Postgres.VariableSetStmt -> noisepage.VariableSetStatement
// std::unique_ptr<VariableSetStatement>
// PostgresParser::VariableSetTransform(ParseResult *parse_result,
//                                      VariableSetStmt *root) {
//   if (root->name == nullptr) {
//     throw PARSER_EXCEPTION("VariableSetTransform: RESET ALL is not
//     supported");
//   }
//   std::string name = root->name;

//   std::vector<shared::ManagedPointer<AbstractExpression>> values;
//   if (name == "SESSION CHARACTERISTICS") {
//     // SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL ...
//     // args is a list of DefElem, each with an A_Const value
//     if (list_length(root->args) != 1) {
//       throw PARSER_EXCEPTION(
//           "VariableSetTransform: set one session characteristic at a time");
//     }
//     auto *def_elem = static_cast<DefElem *>(linitial(root->args));
//     if (def_elem->arg == nullptr || nodeTag(def_elem->arg) != T_A_Const) {
//       throw PARSER_EXCEPTION(
//           "VariableSetTransform: unexpected characteristic value");
//     }
//     name = def_elem->defname;
//     auto expr = ConstTransform(parse_result,
//                                reinterpret_cast<A_Const *>(def_elem->arg));
//     values.emplace_back(shared::ManagedPointer(expr));
//     parse_result->AddExpression(std::move(expr));
//   } else {
//     values = ParamListTransform(parse_result, root->args);
//   }

//   bool is_set_default = root->kind == VAR_SET_DEFAULT;
//   return std::make_unique<VariableSetStatement>(name, std::move(values),
//                                                 is_set_default);
// }

// // Postgres.VariableShowStmt -> noisepage.VariableShowStatement
// std::unique_ptr<VariableShowStatement>
// PostgresParser::VariableShowTransform(ParseResult *parse_result,
//                                       VariableShowStmt *root) {
//   std::string name = root->name;
//   return std::make_unique<VariableShowStatement>(name);
// }

// ---------------------------------------------------------------------------
// WITH (CTEs)
// ---------------------------------------------------------------------------

// Postgres.SelectStmt.withClause -> noisepage.TableRef (one per CTE)
std::vector<std::unique_ptr<TableRef>> PostgresParser::WithTransform(ParseResult *parse_result,
                                                                     WithClause *root) {
  std::vector<std::unique_ptr<TableRef>> ctes{};
  if (root == nullptr) { return ctes; }

  for (int i = 0; i < list_length(root->ctes); i++) {
    auto *cte = static_cast<CommonTableExpr *>(list_nth(root->ctes, i));
    if (cte->ctequery == nullptr || nodeTag(cte->ctequery) != T_SelectStmt) {
      ThrowUnsupported("WithTransform", "CTE query type",
                       cte->ctequery ? static_cast<int>(nodeTag(cte->ctequery)) : -1);
    }
    auto *cte_select_query = reinterpret_cast<SelectStmt *>(cte->ctequery);

    if (root->recursive) {
      // Make left argument the recursive case and right argument the base
      // case, so it is possible to visit the base case without visiting the
      // recursive case (which would otherwise be visited recursively by the
      // visitor in the binder)
      std::swap(cte_select_query->larg, cte_select_query->rarg);
    }

    auto select = SelectTransform(parse_result, cte_select_query);
    if (select == nullptr) { continue; }

    std::vector<AliasType> colnames{};
    for (int j = 0; j < list_length(cte->aliascolnames); j++) {
      colnames.emplace_back(AliasType(std::string(PgStrVal(list_nth(cte->aliascolnames, j))),
                                      alias_oid_t(static_cast<size_t>(j))));
    }

    // WITH ITERATIVE was a NoisePage grammar extension; Postgres only has
    // WITH and WITH RECURSIVE.
    const CteType cte_type =
        root->recursive
            ? (select->HasUnionSelect() ? CteType::STRUCTURALLY_RECURSIVE : CteType::RECURSIVE)
            : CteType::SIMPLE;

    ctes.push_back(TableRef::CreateCTETableRefBySelect(cte->ctename, std::move(select),
                                                       std::move(colnames), cte_type));
  }
  return ctes;
}

} // namespace db7::parser