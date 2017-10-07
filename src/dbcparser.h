#ifndef DBC_PARSER_H
#define DBC_PARSER_H


#include <string>
#include <string_view>
#include <stdexcept>

#include "dbcfile.h"


namespace dbc
{


class Parse_error : public std::runtime_error
{
public:
  Parse_error(const std::string& s) : std::runtime_error{s} {}
  Parse_error(const char* s) : std::runtime_error{s} {}
};


File parse(std::string_view filepath);


}  // namespace dbc


#endif  // DBC_PARSER_H
