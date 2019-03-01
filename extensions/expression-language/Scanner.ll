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

%{
  #include <cerrno>
  #include <climits>
  #include <cstdlib>
  #include <string>
  #include <string>
  #include <sstream>
  #include "Driver.h"
  #include "Parser.hpp"
%}

%option noyywrap
%option nounput
%option batch
%option noinput
%option 8bit
%option c++

id         [a-zA-Z][a-zA-Z_0-9.]*
num        [-]?[0-9]+[.]?[0-9]*([eE][+-]?[0-9]+)?
whitespace [ \r\t]+

%{
  #define YY_USER_ACTION  yylloc->columns(yyleng);
%}

%%

%{
  yylloc->step();
%}

"\n" return Parser::token::TOK_NEWLINE;
"$"  return Parser::token::TOK_DOLLAR;
"{"  return Parser::token::TOK_LCURLY;
"}"  return Parser::token::TOK_RCURLY;
"("  return Parser::token::TOK_LPAREN;
")"  return Parser::token::TOK_RPAREN;
"["  return Parser::token::TOK_LSQUARE;
"]"  return Parser::token::TOK_RSQUARE;
"|"  return Parser::token::TOK_PIPE;
","  return Parser::token::TOK_COMMA;
":"  return Parser::token::TOK_COLON;
";"  return Parser::token::TOK_SEMI;
"/"  return Parser::token::TOK_FSLASH;
"\\" return Parser::token::TOK_BSLASH;
"*"  return Parser::token::TOK_STAR;
"#"  return Parser::token::TOK_HASH;
"'"  return Parser::token::TOK_SQUOTE;
"\"" return Parser::token::TOK_DQUOTE;

{whitespace} {
  yylval->build<std::string>(yytext);
  return Parser::token::TOK_WHITESPACE;
}

{num} {
  yylval->build<std::string>(yytext);
  return Parser::token::TOK_NUMBER;
}

"true" {
  yylval->build<std::string>(yytext);
  return Parser::token::TOK_TRUE;
}

"false" {
  yylval->build<std::string>(yytext);
  return Parser::token::TOK_FALSE;
}

{id} {
  yylval->build<std::string>(yytext);
  return Parser::token::TOK_IDENTIFIER;
}

. {
  yylval->build<std::string>(yytext);
  return Parser::token::TOK_MISC;
}

<<EOF>> return Parser::token::TOK_END;

%%

int yyFlexLexer::yylex() {
  throw std::logic_error("Not implemented.");
}
