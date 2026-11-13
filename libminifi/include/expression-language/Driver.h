// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <map>
#include "expression-language/Expression.h"

#undef yyFlexLexer
#include <FlexLexer.h>
#include "Parser.hpp"

#undef YY_DECL
#define YY_DECL int org::apache::nifi::minifi::expression::Driver::lex(org::apache::nifi::minifi::expression::Parser::semantic_type* yylval, \
                                                                       org::apache::nifi::minifi::expression::Parser::location_type* yylloc)

namespace org::apache::nifi::minifi::expression {

class Driver : public yyFlexLexer {
 public:
  explicit Driver(std::istream *input = nullptr, std::ostream *output = nullptr)
      : yyFlexLexer(input, output),
        result(Value()) {
  }
  ~Driver() override = default;
  int lex(Parser::semantic_type *yylval,
          Parser::location_type *yylloc);

  std::map<std::string, int> variables;

  Expression result;
};

}  // namespace org::apache::nifi::minifi::expression
