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

%skeleton "lalr1.cc"
%require "3.0"

%define api.namespace {org::apache::nifi::minifi::expression}
%parse-param {Driver* driver} 
%locations

%define parser_class_name {Parser}
%define parse.error verbose
%define api.value.type variant
%define parse.assert
%define api.token.prefix {TOK_}

%code requires
{
  #include <string>

  #include <expression/Expression.h>
  #include "location.hh"

  namespace org {
  namespace apache {
  namespace nifi {
  namespace minifi {
  namespace expression {
    class Driver;
  } /* namespace expression */
  } /* namespace minifi */
  } /* namespace nifi */
  } /* namespace apache */
  } /* namespace org */
}

%code
{
  #include <utility>
  #include <memory>
  #include <functional>

  #include <expression/Expression.h>
  #include "Driver.h"
  
  #undef yylex
  #define yylex driver->lex
}

%token
  END 0          "eof"
  NEWLINE        "\n"
  DOLLAR         "$"
  LCURLY         "{"
  RCURLY         "}"
  LPAREN         "("
  RPAREN         ")"
  LSQUARE        "["
  RSQUARE        "]"
  PIPE           "|"
  MINUS          "-"
  COMMA          ","
  COLON          ":"
  SEMI           ";"
  FSLASH         "/"
  BSLASH         "\\"
  STAR           "*"
  HASH           "#"
  SQUOTE         "'"
  DQUOTE         "\""
;

%token <std::string> TRUE       "true"
%token <std::string> FALSE      "false"
%token <std::string> IDENTIFIER "identifier"
%token <std::string> MISC       "misc"
%token <std::string> WHITESPACE "whitespace"
%token <std::string> NUMBER     "number"

%type <std::string>             exp_whitespace
%type <std::string>             exp_whitespaces
%type <std::string>             attr_id
%type <Expression>              fn_call
%type <Expression>              fn_arg
%type <std::vector<Expression>> fn_args
%type <Expression>              exp_content
%type <Expression>              exp_content_val
%type <Expression>              exp
%type <Expression>              exps
%type <std::string>             text_no_quote_no_dollar
%type <std::string>             text_inc_quote_escaped_dollar
%type <std::string>             text_inc_dollar
%type <std::string>             quoted_text
%type <std::string>             quoted_text_content
%type <Expression>              text_or_exp

%type <std::vector<std::pair<std::string, std::vector<Expression>>>> fn_calls

%%

%start root;

root: exps { driver->result = $1; }
    ;

text_no_quote_no_dollar: IDENTIFIER { std::swap($$, $1); }
                       | WHITESPACE { std::swap($$, $1); }
                       | NEWLINE { $$ = "\n"; }
                       | MISC { std::swap($$, $1); }
                       | LCURLY { $$ = "{"; }
                       | RCURLY { $$ = "}"; }
                       | LPAREN { $$ = "("; }
                       | RPAREN { $$ = ")"; }
                       | LSQUARE { $$ = "["; }
                       | RSQUARE { $$ = "]"; }
                       | PIPE { $$ = "|"; }
                       | COMMA { $$ = ","; }
                       | COLON { $$ = ":"; }
                       | TRUE { $$ = std::string("true"); }
                       | FALSE { $$ = std::string("false"); }
                       | SEMI { $$ = ";"; }
                       | FSLASH { $$ = "/"; }
                       | STAR { $$ = "*"; }
                       | HASH { $$ = "#"; }
                       | NUMBER { std::swap($$, $1); }
                       ;

text_inc_quote_escaped_dollar: text_no_quote_no_dollar { std::swap($$, $1); }
                             | SQUOTE { $$ = "'"; }
                             | DQUOTE { $$ = "\""; }
                             | BSLASH { $$ = "\\"; }
                             | DOLLAR DOLLAR { $$ = "$"; }
                             ;

text_inc_dollar: text_no_quote_no_dollar { std::swap($$, $1); }
               | DOLLAR { $$ = "$"; }
               | BSLASH SQUOTE { $$ = "'"; }
               | BSLASH DQUOTE { $$ = "\""; }
               | BSLASH BSLASH { $$ = "\\"; }
               ;

quoted_text_content: %empty {}
                   | quoted_text_content text_inc_dollar { $$ = $1 + $2; }
                   ;

quoted_text: SQUOTE quoted_text_content SQUOTE { std::swap($$, $2); }
           | DQUOTE quoted_text_content DQUOTE { std::swap($$, $2); }
           ;

text_or_exp: text_inc_quote_escaped_dollar { $$ = make_static(std::move($1)); }
           | exp { $$ = $1; }
           ;

exps: %empty {}
    | exps text_or_exp { $$ = $1 + $2; }
    ;

exp_whitespace: WHITESPACE {}
              | NEWLINE {}
              ;

exp_whitespaces: %empty {}
               | exp_whitespaces exp_whitespace {}
               ;

attr_id: quoted_text exp_whitespaces { std::swap($$, $1); }
       | IDENTIFIER exp_whitespaces { std::swap($$, $1); }
       ;

fn_arg: quoted_text exp_whitespaces { $$ = make_static($1); }
      | NUMBER exp_whitespaces { $$ = make_static($1); }
      | TRUE { $$ = Expression(Value(true)); }
      | FALSE { $$ = Expression(Value(false)); }
      | exp exp_whitespaces { $$ = $1; }
      ;

fn_args: %empty {}
       | fn_args COMMA exp_whitespaces fn_arg { $$.insert($$.end(), $1.begin(), $1.end()); $$.push_back($4); }
       | fn_arg { $$.push_back($1); }

fn_call: attr_id LPAREN exp_whitespaces fn_args RPAREN exp_whitespaces { $$ = make_dynamic_function(std::move($1), $4); }

fn_calls: %empty {}
        | COLON exp_whitespaces attr_id LPAREN exp_whitespaces fn_args RPAREN exp_whitespaces fn_calls { $$.push_back({$3, $6}); $$.insert($$.end(), $9.begin(), $9.end()); }
        ;

exp_content_val: attr_id { $$ = make_dynamic_attr(std::move($1)); }
               | fn_call { $$ = $1; }
               ;

exp_content: exp_content_val fn_calls { $$ = make_function_composition($1, $2); }
           ;

exp: DOLLAR LCURLY exp_whitespaces exp_content RCURLY { $$ = $4; }
   ;

%%

void org::apache::nifi::minifi::expression::Parser::error(const location_type &location,
                                                          const std::string &message) {
  std::stringstream err_msg;
  err_msg << location;
  err_msg << ": ";
  err_msg << message;
  throw std::runtime_error(err_msg.str());
}
