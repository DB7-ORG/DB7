#include "postgres_parser.hpp"
#include "third_party/libpg_query/pg_query.h"
#include "third_party/libpg_query/pg_list.h"

#include <fmt/format.h>

namespace noisepage::parser
{

    std::unique_ptr<parser::ParseResult> PostgresParser::BuildParseTree(const std::string &query_string)
    {
        auto text = query_string.c_str();
        auto ctx = pg_query_parse_init();
        auto result = pg_query_parse(text);

        // Parse the query string with the Postgres parser.
        if (result.error != nullptr)
        {
            std::string msg = fmt::format("Parse error: {} at position {}",
                                          result.error->message, result.error->cursorpos);
            pg_query_parse_finish(ctx);
            pg_query_free_parse_result(result);
            throw std::runtime_error(msg);
        }

        // fmt::print("Parse tree stderr_str: {}\n", result.stderr_buffer ? result.stderr_buffer : "(null)");
        if (result.tree != nullptr)
        {
            for (auto cell = result.tree->head; cell != nullptr; cell = cell->next)
            {
                auto node = static_cast<Node *>(cell->data.ptr_value);
                fmt::print("Node type: {}\n", node->type);
            }
        }

        // Transform the Postgres parse tree to a Terrier representation.
        // auto parse_result = std::make_unique<ParseResult>();
        // try
        // {
        //     ListTransform(parse_result.get(), result.tree);
        // }
        // catch (const Exception &e)
        // {
        //     pg_query_parse_finish(ctx);
        //     pg_query_free_parse_result(result);
        //     PARSER_LOG_DEBUG("BuildParseTree: caught {} {} {} {}", e.GetType(), e.GetFile(), e.GetLine(), e.what());
        //     throw;
        // }

        pg_query_parse_finish(ctx);
        pg_query_free_parse_result(result);
        // return parse_result;

        return nullptr;
    }

    // void PostgresParser::ListTransform(ParseResult *parse_result, List *root)
    // {
    //     if (root != nullptr)
    //     {
    //         for (auto cell = root->head; cell != nullptr; cell = cell->next)
    //         {
    //             auto node = static_cast<Node *>(cell->data.ptr_value);
    //             parse_result->AddStatement(NodeTransform(parse_result, node));
    //         }
    //     }
    // }

}