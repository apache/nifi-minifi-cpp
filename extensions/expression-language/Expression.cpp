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

#include <utility>
#include <iostream>
#include <iomanip>
#include <random>
#include <algorithm>

#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

#include <utils/StringUtils.h>
#include <expression/Expression.h>
#include <regex>
#include "Driver.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace expression {

Expression compile(const std::string &expr_str) {
  std::stringstream expr_str_stream(expr_str);
  Driver driver(&expr_str_stream);
  Parser parser(&driver);
  parser.parse();
  return driver.result;
}

Expression make_static(std::string val) {
  return Expression(Value(val));
}

Expression make_dynamic(const std::function<Value(const Parameters &params)> &val_fn) {
  return Expression(Value(), val_fn);
}

Expression make_dynamic_attr(const std::string &attribute_id) {
  return make_dynamic([attribute_id](const Parameters &params) -> Value {
    std::string result;
    if (params.flow_file.lock()->getAttribute(attribute_id, result)) {
      return Value(result);
    } else {
      return Value();
    }
  });
}

Value expr_hostname(const std::vector<Value> &args) {
  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);
  return Value(std::string(hostname));
}

Value expr_toUpper(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return Value(result);
}

Value expr_toLower(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return Value(result);
}

Value expr_substring(const std::vector<Value> &args) {
  if (args.size() < 3) {
    return Value(args[0].asString().substr(args[1].asUnsignedLong()));
  } else {
    return Value(args[0].asString().substr(args[1].asUnsignedLong(), args[2].asUnsignedLong()));
  }
}

Value expr_substringBefore(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  return Value(arg_0.substr(0, arg_0.find(args[1].asString())));
}

Value expr_substringBeforeLast(const std::vector<Value> &args) {
  size_t last_pos = 0;
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  while (arg_0.find(arg_1, last_pos + 1) != std::string::npos) {
    last_pos = arg_0.find(arg_1, last_pos + 1);
  }
  return Value(arg_0.substr(0, last_pos));
}

Value expr_substringAfter(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  return Value(arg_0.substr(arg_0.find(arg_1) + arg_1.length()));
}

Value expr_substringAfterLast(const std::vector<Value> &args) {
  size_t last_pos = 0;
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  while (arg_0.find(arg_1, last_pos + 1) != std::string::npos) {
    last_pos = arg_0.find(arg_1, last_pos + 1);
  }
  return Value(arg_0.substr(last_pos + arg_1.length()));
}

Value expr_startsWith(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  return Value(arg_0.substr(0, arg_1.length()) == arg_1);
}

Value expr_endsWith(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  return Value(arg_0.substr(arg_0.length() - arg_1.length()) == arg_1);
}

Value expr_contains(const std::vector<Value> &args) {
  return Value(std::string::npos != args[0].asString().find(args[1].asString()));
}

Value expr_in(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  for (size_t i = 1; i < args.size(); i++) {
    if (arg_0 == args[i].asString()) {
      return Value(true);
    }
  }

  return Value(false);
}

Value expr_indexOf(const std::vector<Value> &args) {
  auto pos = args[0].asString().find(args[1].asString());

  if (pos == std::string::npos) {
    return Value(static_cast<int64_t >(-1));
  } else {
    return Value(static_cast<int64_t >(pos));
  }
}

Value expr_lastIndexOf(const std::vector<Value> &args) {
  size_t pos = std::string::npos;
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  auto cur_pos = arg_0.find(arg_1, 0);

  while (cur_pos != std::string::npos) {
    pos = cur_pos;
    cur_pos = arg_0.find(arg_1, pos + 1);
  }

  if (pos == std::string::npos) {
    return Value(static_cast<int64_t >(-1));
  } else {
    return Value(static_cast<int64_t >(pos));
  }
}

Value expr_escapeJson(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  writer.String(arg_0.c_str());
  std::string result(buf.GetString());
  return Value(result.substr(1, result.length() - 2));
}

Value expr_unescapeJson(const std::vector<Value> &args) {
  std::stringstream arg_0_ss;
  arg_0_ss << "[\"" << args[0].asString() << "\"]";
  rapidjson::Reader reader;
  rapidjson::Document doc;
  doc.Parse(arg_0_ss.str().c_str());
  if (doc.IsArray() && doc.Size() == 1 && doc[0].IsString()) {
    return Value(std::string(doc[0].GetString()));
  } else {
    return Value();
  }
}

Value expr_escapeXml(const std::vector<Value> &args) {
  return Value(utils::StringUtils::replaceMap(
      args[0].asString(),
      {
          {"\"", "&quot;"},
          {"'", "&apos;"},
          {"<", "&lt;"},
          {">", "&gt;"},
          {"&", "&amp;"}
      }));
}

Value expr_unescapeXml(const std::vector<Value> &args) {
  return Value(utils::StringUtils::replaceMap(
      args[0].asString(),
      {
          {"&quot;", "\""},
          {"&apos;", "'"},
          {"&lt;", "<"},
          {"&gt;", ">"},
          {"&amp;", "&"}
      }));
}

#ifdef EXPRESSION_LANGUAGE_USE_REGEX

Value expr_replace(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  const std::string &find = args[1].asString();
  const std::string &replace = args[2].asString();

  std::string::size_type match_pos = 0;
  match_pos = result.find(find, match_pos);

  while (match_pos != std::string::npos) {
    result.replace(match_pos, find.size(), replace);
    match_pos = result.find(find, match_pos + replace.size());
  }

  return Value(result);
}

Value expr_replaceFirst(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  const std::regex find(args[1].asString());
  const std::string &replace = args[2].asString();
  return Value(std::regex_replace(result, find, replace, std::regex_constants::format_first_only));
}

Value expr_replaceAll(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  const std::regex find(args[1].asString());
  const std::string &replace = args[2].asString();
  return Value(std::regex_replace(result, find, replace));
}

Value expr_replaceNull(const std::vector<Value> &args) {
  if (args[0].isNull()) {
    return args[1];
  } else {
    return args[0];
  }
}

Value expr_replaceEmpty(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  const std::regex find("^[ \n\r\t]*$");
  const std::string &replace = args[1].asString();
  return Value(std::regex_replace(result, find, replace));
}

Value expr_matches(const std::vector<Value> &args) {
  const auto &subject = args[0].asString();
  const std::regex expr = std::regex(args[1].asString());

  return Value(std::regex_match(subject.begin(), subject.end(), expr));
}

Value expr_find(const std::vector<Value> &args) {
  const auto &subject = args[0].asString();
  const std::regex expr = std::regex(args[1].asString());

  return Value(std::regex_search(subject.begin(), subject.end(), expr));
}

#endif  // EXPRESSION_LANGUAGE_USE_REGEX

Value expr_binary_op(const std::vector<Value> &args,
                     long double (*ldop)(long double, long double),
                     int64_t (*iop)(int64_t, int64_t),
                     bool long_only = false) {
  try {
    if (!long_only && !args[0].isDecimal() && !args[1].isDecimal()) {
      return Value(iop(args[0].asSignedLong(), args[1].asSignedLong()));
    } else {
      return Value(ldop(args[0].asLongDouble(), args[1].asLongDouble()));
    }
  } catch (const std::exception &e) {
    return Value();
  }
}

Value expr_plus(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return a + b; },
                        [](int64_t a, int64_t b) { return a + b; });
}

Value expr_minus(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return a - b; },
                        [](int64_t a, int64_t b) { return a - b; });
}

Value expr_multiply(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return a * b; },
                        [](int64_t a, int64_t b) { return a * b; });
}

Value expr_divide(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return a / b; },
                        [](int64_t a, int64_t b) { return a / b; },
                        true);
}

Value expr_mod(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return std::fmod(a, b); },
                        [](int64_t a, int64_t b) { return a % b; });
}

Value expr_toRadix(const std::vector<Value> &args) {
  int64_t radix = args[1].asSignedLong();

  if (radix < 2 || radix > 36) {
    throw std::runtime_error("Cannot perform conversion due to invalid radix");
  }

  int pad_width = 0;

  if (args.size() > 2) {
    pad_width = static_cast<int>(args[2].asUnsignedLong());
  }

//  auto value = std::stoll(args[0].asString(), nullptr, 10);
  auto value = args[0].asSignedLong();

  std::string sign;

  if (value < 0) {
    sign = "-";
  }

  const char chars[] =
      "0123456789ab"
          "cdefghijklmn"
          "opqrstuvwxyz";
  std::string str_num;

  while (value) {
    str_num += chars[std::abs(value % radix)];
    value /= radix;
  }

  std::reverse(str_num.begin(), str_num.end());

  std::stringstream ss;
  ss << sign << std::setfill('0') << std::setw(pad_width) << str_num;
  return Value(ss.str());
}

Value expr_fromRadix(const std::vector<Value> &args) {
  auto radix = args[1].asSignedLong();

  if (radix < 2 || radix > 36) {
    throw std::runtime_error("Cannot perform conversion due to invalid radix");
  }

  return Value(std::to_string(std::stoll(args[0].asString(), nullptr, radix)));
}

Value expr_random(const std::vector<Value> &args) {
  std::random_device random_device;
  std::mt19937 generator(random_device());
  std::uniform_int_distribution<int64_t> distribution(0, LLONG_MAX);
  return Value(distribution(generator));
}

template<Value T(const std::vector<Value> &)>
Expression make_dynamic_function_incomplete(const std::string &function_name,
                                            const std::vector<Expression> &args,
                                            std::size_t num_args) {

  if (args.size() < num_args) {
    std::stringstream message_ss;
    message_ss << "Expression language function "
               << function_name
               << " called with "
               << args.size()
               << " argument(s), but "
               << num_args
               << " are required";
    throw std::runtime_error(message_ss.str());
  }

  auto result = make_dynamic([=](const Parameters &params) -> Value {
    std::vector<Value> evaluated_args;

    for (const auto &arg : args) {
      evaluated_args.emplace_back(arg(params));
    }

    return T(evaluated_args);
  });

  return result;
}

Value expr_literal(const std::vector<Value> &args) {
  return args[0];
}

Value expr_isNull(const std::vector<Value> &args) {
  return Value(args[0].isNull());
}

Value expr_notNull(const std::vector<Value> &args) {
  return Value(!args[0].isNull());
}

Value expr_isEmpty(const std::vector<Value> &args) {
  if (args[0].isNull()) {
    return Value(true);
  }

  std::string arg_0 = args[0].asString();

  for (char c : arg_0) {
    if (c != ' '
        && c != '\f'
        && c != '\n'
        && c != '\r'
        && c != '\t'
        && c != '\v') {
      return Value(false);
    }
  }

  return Value(true);
}

Value expr_equals(const std::vector<Value> &args) {
  return Value(args[0].asString() == args[1].asString());
}

Value expr_equalsIgnoreCase(const std::vector<Value> &args) {
  auto arg_0 = args[0].asString();
  auto arg_1 = args[1].asString();

  std::transform(arg_0.begin(), arg_0.end(), arg_0.begin(), ::tolower);
  std::transform(arg_1.begin(), arg_1.end(), arg_1.begin(), ::tolower);

  return Value(arg_0 == arg_1);
}

Value expr_gt(const std::vector<Value> &args) {
  if (args[0].isDecimal() && args[1].isDecimal()) {
    return Value(args[0].asLongDouble() > args[1].asLongDouble());
  } else {
    return Value(args[0].asSignedLong() > args[1].asSignedLong());
  }
}

Value expr_ge(const std::vector<Value> &args) {
  if (args[0].isDecimal() && args[1].isDecimal()) {
    return Value(args[0].asLongDouble() >= args[1].asLongDouble());
  } else {
    return Value(args[0].asSignedLong() >= args[1].asSignedLong());
  }
}

Value expr_lt(const std::vector<Value> &args) {
  if (args[0].isDecimal() && args[1].isDecimal()) {
    return Value(args[0].asLongDouble() < args[1].asLongDouble());
  } else {
    return Value(args[0].asSignedLong() < args[1].asSignedLong());
  }
}

Value expr_le(const std::vector<Value> &args) {
  if (args[0].isDecimal() && args[1].isDecimal()) {
    return Value(args[0].asLongDouble() <= args[1].asLongDouble());
  } else {
    return Value(args[0].asSignedLong() <= args[1].asSignedLong());
  }
}

Value expr_and(const std::vector<Value> &args) {
  return Value(args[0].asBoolean() && args[1].asBoolean());
}

Value expr_or(const std::vector<Value> &args) {
  return Value(args[0].asBoolean() || args[1].asBoolean());
}

Value expr_not(const std::vector<Value> &args) {
  return Value(!args[0].asBoolean());
}

Value expr_ifElse(const std::vector<Value> &args) {
  if (args[0].asBoolean()) {
    return args[1];
  } else {
    return args[2];
  }
}

Expression make_dynamic_function(const std::string &function_name,
                                 const std::vector<Expression> &args) {
  if (function_name == "hostname") {
    return make_dynamic_function_incomplete<expr_hostname>(function_name, args, 0);
  } else if (function_name == "toUpper") {
    return make_dynamic_function_incomplete<expr_toUpper>(function_name, args, 1);
  } else if (function_name == "toLower") {
    return make_dynamic_function_incomplete<expr_toLower>(function_name, args, 1);
  } else if (function_name == "substring") {
    return make_dynamic_function_incomplete<expr_substring>(function_name, args, 2);
  } else if (function_name == "substringBefore") {
    return make_dynamic_function_incomplete<expr_substringBefore>(function_name, args, 2);
  } else if (function_name == "substringBeforeLast") {
    return make_dynamic_function_incomplete<expr_substringBeforeLast>(function_name, args, 2);
  } else if (function_name == "substringAfter") {
    return make_dynamic_function_incomplete<expr_substringAfter>(function_name, args, 2);
  } else if (function_name == "substringAfterLast") {
    return make_dynamic_function_incomplete<expr_substringAfterLast>(function_name, args, 2);
  } else if (function_name == "startsWith") {
    return make_dynamic_function_incomplete<expr_startsWith>(function_name, args, 1);
  } else if (function_name == "endsWith") {
    return make_dynamic_function_incomplete<expr_endsWith>(function_name, args, 1);
  } else if (function_name == "contains") {
    return make_dynamic_function_incomplete<expr_contains>(function_name, args, 1);
  } else if (function_name == "in") {
    return make_dynamic_function_incomplete<expr_in>(function_name, args, 1);
  } else if (function_name == "indexOf") {
    return make_dynamic_function_incomplete<expr_indexOf>(function_name, args, 1);
  } else if (function_name == "lastIndexOf") {
    return make_dynamic_function_incomplete<expr_lastIndexOf>(function_name, args, 1);
  } else if (function_name == "escapeJson") {
    return make_dynamic_function_incomplete<expr_escapeJson>(function_name, args, 0);
  } else if (function_name == "unescapeJson") {
    return make_dynamic_function_incomplete<expr_unescapeJson>(function_name, args, 0);
  } else if (function_name == "escapeXml") {
    return make_dynamic_function_incomplete<expr_escapeXml>(function_name, args, 0);
  } else if (function_name == "unescapeXml") {
    return make_dynamic_function_incomplete<expr_unescapeXml>(function_name, args, 0);
#ifdef EXPRESSION_LANGUAGE_USE_REGEX
  } else if (function_name == "replace") {
    return make_dynamic_function_incomplete<expr_replace>(function_name, args, 2);
  } else if (function_name == "replaceFirst") {
    return make_dynamic_function_incomplete<expr_replaceFirst>(function_name, args, 2);
  } else if (function_name == "replaceAll") {
    return make_dynamic_function_incomplete<expr_replaceAll>(function_name, args, 2);
  } else if (function_name == "replaceNull") {
    return make_dynamic_function_incomplete<expr_replaceNull>(function_name, args, 1);
  } else if (function_name == "replaceEmpty") {
    return make_dynamic_function_incomplete<expr_replaceEmpty>(function_name, args, 1);
  } else if (function_name == "matches") {
    return make_dynamic_function_incomplete<expr_matches>(function_name, args, 1);
  } else if (function_name == "find") {
    return make_dynamic_function_incomplete<expr_find>(function_name, args, 1);
#endif  // EXPRESSION_LANGUAGE_USE_REGEX
  } else if (function_name == "plus") {
    return make_dynamic_function_incomplete<expr_plus>(function_name, args, 1);
  } else if (function_name == "minus") {
    return make_dynamic_function_incomplete<expr_minus>(function_name, args, 1);
  } else if (function_name == "multiply") {
    return make_dynamic_function_incomplete<expr_multiply>(function_name, args, 1);
  } else if (function_name == "divide") {
    return make_dynamic_function_incomplete<expr_divide>(function_name, args, 1);
  } else if (function_name == "mod") {
    return make_dynamic_function_incomplete<expr_mod>(function_name, args, 1);
  } else if (function_name == "fromRadix") {
    return make_dynamic_function_incomplete<expr_fromRadix>(function_name, args, 2);
  } else if (function_name == "toRadix") {
    return make_dynamic_function_incomplete<expr_toRadix>(function_name, args, 1);
  } else if (function_name == "random") {
    return make_dynamic_function_incomplete<expr_random>(function_name, args, 0);
  } else if (function_name == "literal") {
    return make_dynamic_function_incomplete<expr_literal>(function_name, args, 1);
  } else if (function_name == "isNull") {
    return make_dynamic_function_incomplete<expr_isNull>(function_name, args, 0);
  } else if (function_name == "notNull") {
    return make_dynamic_function_incomplete<expr_notNull>(function_name, args, 0);
  } else if (function_name == "isEmpty") {
    return make_dynamic_function_incomplete<expr_isEmpty>(function_name, args, 0);
  } else if (function_name == "equals") {
    return make_dynamic_function_incomplete<expr_equals>(function_name, args, 1);
  } else if (function_name == "equalsIgnoreCase") {
    return make_dynamic_function_incomplete<expr_equalsIgnoreCase>(function_name, args, 1);
  } else if (function_name == "gt") {
    return make_dynamic_function_incomplete<expr_gt>(function_name, args, 1);
  } else if (function_name == "ge") {
    return make_dynamic_function_incomplete<expr_ge>(function_name, args, 1);
  } else if (function_name == "lt") {
    return make_dynamic_function_incomplete<expr_lt>(function_name, args, 1);
  } else if (function_name == "le") {
    return make_dynamic_function_incomplete<expr_le>(function_name, args, 1);
  } else if (function_name == "and") {
    return make_dynamic_function_incomplete<expr_and>(function_name, args, 1);
  } else if (function_name == "or") {
    return make_dynamic_function_incomplete<expr_or>(function_name, args, 1);
  } else if (function_name == "not") {
    return make_dynamic_function_incomplete<expr_not>(function_name, args, 0);
  } else if (function_name == "ifElse") {
    return make_dynamic_function_incomplete<expr_ifElse>(function_name, args, 2);
  } else {
    std::string msg("Unknown expression function: ");
    msg.append(function_name);
    throw std::runtime_error(msg);
  }
}

Expression make_function_composition(const Expression &arg,
                                     const std::vector<std::pair<std::string, std::vector<Expression>>> &chain) {

  auto expr = arg;

  for (const auto &chain_part : chain) {
    std::vector<Expression> complete_args = {expr};
    complete_args.insert(complete_args.end(), chain_part.second.begin(), chain_part.second.end());
    expr = make_dynamic_function(chain_part.first, complete_args);
  }

  return expr;
}

bool Expression::isDynamic() const {
  if (val_fn_) {
    return true;
  } else {
    return false;
  }
}

Expression Expression::operator+(const Expression &other_expr) const {
  if (isDynamic() && other_expr.isDynamic()) {
    auto val_fn = val_fn_;
    auto other_val_fn = other_expr.val_fn_;
    return make_dynamic([val_fn, other_val_fn](const Parameters &params) -> Value {
      Value result = val_fn(params);
      return Value(result.asString().append(other_val_fn(params).asString()));
    });
  } else if (isDynamic() && !other_expr.isDynamic()) {
    auto val_fn = val_fn_;
    auto other_val = other_expr.val_;
    return make_dynamic([val_fn, other_val](const Parameters &params) -> Value {
      Value result = val_fn(params);
      return Value(result.asString().append(other_val.asString()));
    });
  } else if (!isDynamic() && other_expr.isDynamic()) {
    auto val = val_;
    auto other_val_fn = other_expr.val_fn_;
    return make_dynamic([val, other_val_fn](const Parameters &params) -> Value {
      Value result(val);
      return Value(result.asString().append(other_val_fn(params).asString()));
    });
  } else if (!isDynamic() && !other_expr.isDynamic()) {
    std::string result(val_.asString());
    result.append(other_expr.val_.asString());
    return make_static(result);
  } else {
    throw std::runtime_error("Invalid function composition");
  }
}

Value Expression::operator()(const Parameters &params) const {
  if (isDynamic()) {
    return val_fn_(params);
  } else {
    return val_;
  }
}

} /* namespace expression */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
