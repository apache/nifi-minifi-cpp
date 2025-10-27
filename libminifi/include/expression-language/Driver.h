/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
