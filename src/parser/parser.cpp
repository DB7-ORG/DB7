#include "parser.hpp"

u32 Parse()
{
    std::string query = "SELECT * FROM customers";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(query, &result);

    if (result.isValid())
    {
        printf("Parsed successfully!\n");
        printf("Number of statements: %lu\n", result.size());
    }
    else
    {
        printf("The SQL string is invalid!\n");
        return -1;
    }
    return 0;
}