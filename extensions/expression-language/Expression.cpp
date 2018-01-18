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
#include <string>
#include <random>

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
  return Expression(std::move(val));
}

Expression make_dynamic(std::function<std::string(const Parameters &params)> val_fn) {
  return Expression("", std::move(val_fn));
}

Expression make_dynamic_attr(const std::string &attribute_id) {
  return make_dynamic([attribute_id](const Parameters &params) -> std::string {
    std::string result;
    params.flow_file.lock()->getAttribute(attribute_id, result);
    return result;
  });
}

std::string expr_hostname(const std::vector<std::string> &args) {
  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);
  return std::string(hostname);
}

std::string expr_toUpper(const std::vector<std::string> &args) {
  std::string result = args[0];
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

std::string expr_substring(const std::vector<std::string> &args) {
  if (args.size() < 3) {
    return args[0].substr(std::stoul(args[1]));
  } else {
    return args[0].substr(std::stoul(args[1]), std::stoul(args[2]));
  }
}

std::string expr_substringBefore(const std::vector<std::string> &args) {
  return args[0].substr(0, args[0].find(args[1]));
}

std::string expr_substringBeforeLast(const std::vector<std::string> &args) {
  size_t last_pos = 0;
  while (args[0].find(args[1], last_pos + 1) != std::string::npos) {
    last_pos = args[0].find(args[1], last_pos + 1);
  }
  return args[0].substr(0, last_pos);
}

std::string expr_substringAfter(const std::vector<std::string> &args) {
  return args[0].substr(args[0].find(args[1]) + args[1].length());
}

std::string expr_substringAfterLast(const std::vector<std::string> &args) {
  size_t last_pos = 0;
  while (args[0].find(args[1], last_pos + 1) != std::string::npos) {
    last_pos = args[0].find(args[1], last_pos + 1);
  }
  return args[0].substr(last_pos + args[1].length());
}

#ifdef EXPRESSION_LANGUAGE_USE_REGEX

std::string expr_replace(const std::vector<std::string> &args) {
  std::string result = args[0];
  const std::string find = args[1];
  const std::string replace = args[2];

  std::string::size_type match_pos = 0;
  match_pos = result.find(find, match_pos);

  while (match_pos != std::string::npos) {
    result.replace(match_pos, find.size(), replace);
    match_pos = result.find(find, match_pos + replace.size());
  }

  return result;
}

std::string expr_replaceFirst(const std::vector<std::string> &args) {
  std::string result = args[0];
  const std::regex find(args[1]);
  const std::string replace = args[2];
  return std::regex_replace(result, find, replace, std::regex_constants::format_first_only);
}

std::string expr_replaceAll(const std::vector<std::string> &args) {
  std::string result = args[0];
  const std::regex find(args[1]);
  const std::string replace = args[2];
  return std::regex_replace(result, find, replace);
}

std::string expr_replaceNull(const std::vector<std::string> &args) {
  if (args[0].empty()) {
    return args[1];
  } else {
    return args[0];
  }
}

std::string expr_replaceEmpty(const std::vector<std::string> &args) {
  std::string result = args[0];
  const std::regex find("^[ \n\r\t]*$");
  const std::string replace = args[1];
  return std::regex_replace(result, find, replace);
}

#endif  // EXPRESSION_LANGUAGE_USE_REGEX

std::string expr_binaryOp(const std::vector<std::string> &args,
                          long double (*ldop)(long double, long double),
                          int (*iop)(int, int),
                          bool long_only = false) {
  try {
    if (!long_only &&
        args[0].find('.') == args[0].npos &&
        args[1].find('.') == args[1].npos &&
        args[1].find('e') == args[1].npos &&
        args[0].find('e') == args[0].npos &&
        args[0].find('E') == args[0].npos &&
        args[1].find('E') == args[1].npos) {
      return std::to_string(iop(std::stoi(args[0]), std::stoi(args[1])));
    } else {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(std::numeric_limits<double>::digits10)
         << ldop(std::stold(args[0]), std::stold(args[1]));
      auto result = ss.str();
      result.erase(result.find_last_not_of('0') + 1, std::string::npos);

      if (result.find('.') == result.length() - 1) {
        result.erase(result.length() - 1, std::string::npos);
      }

      return result;
    }
  } catch (const std::exception &e) {
    return "";
  }
}

std::string expr_plus(const std::vector<std::string> &args) {
  return expr_binaryOp(args,
                       [](long double a, long double b) { return a + b; },
                       [](int a, int b) { return a + b; });
}

std::string expr_minus(const std::vector<std::string> &args) {
  return expr_binaryOp(args,
                       [](long double a, long double b) { return a - b; },
                       [](int a, int b) { return a - b; });
}

std::string expr_multiply(const std::vector<std::string> &args) {
  return expr_binaryOp(args,
                       [](long double a, long double b) { return a * b; },
                       [](int a, int b) { return a * b; });
}

std::string expr_divide(const std::vector<std::string> &args) {
  return expr_binaryOp(args,
                       [](long double a, long double b) { return a / b; },
                       [](int a, int b) { return a / b; },
                       true);
}

std::string expr_mod(const std::vector<std::string> &args) {
  return expr_binaryOp(args,
                       [](long double a, long double b) { return std::fmod(a, b); },
                       [](int a, int b) { return a % b; });
}

std::string expr_toRadix(const std::vector<std::string> &args) {
  int radix = std::stoi(args[1]);

  if (radix < 2 || radix > 36) {
    throw std::runtime_error("Cannot perform conversion due to invalid radix");
  }

  int pad_width = 0;

  if (args.size() > 2) {
    pad_width = std::stoi(args[2]);
  }

  auto value = std::stoll(args[0], nullptr, 10);

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
  return ss.str();
}

std::string expr_fromRadix(const std::vector<std::string> &args) {
  int radix = std::stoi(args[1]);

  if (radix < 2 || radix > 36) {
    throw std::runtime_error("Cannot perform conversion due to invalid radix");
  }

  return std::to_string(std::stoll(args[0], nullptr, radix));
}

std::string expr_random(const std::vector<std::string> &args) {
  std::random_device random_device;
  std::mt19937 generator(random_device());
  std::uniform_int_distribution<long long> distribution(0, LLONG_MAX);
  return std::to_string(distribution(generator));
}

template<std::string T(const std::vector<std::string> &)>
Expression make_dynamic_function_incomplete(const std::string &function_name,
                                            const std::vector<Expression> &args,
                                            std::size_t num_args) {
  if (args.size() >= num_args) {
    auto result = make_dynamic([=](const Parameters &params) -> std::string {
      std::vector<std::string> evaluated_args;

      for (const auto &arg : args) {
        evaluated_args.emplace_back(arg(params));
      }

      return T(evaluated_args);
    });

    result.complete = [function_name, args](Expression expr) -> Expression {
      std::vector<Expression> complete_args = {expr};
      complete_args.insert(complete_args.end(), args.begin(), args.end());
      return make_dynamic_function(function_name, complete_args);
    };

    return result;
  } else {
    auto result = make_dynamic([](const Parameters &params) -> std::string {
      throw std::runtime_error("Attempted to call incomplete function");
    });

    result.complete = [function_name, args](Expression expr) -> Expression {
      std::vector<Expression> complete_args = {expr};
      complete_args.insert(complete_args.end(), args.begin(), args.end());
      return make_dynamic_function(function_name, complete_args);
    };

    return result;
  }
}

Expression make_dynamic_function(const std::string &function_name,
                                 const std::vector<Expression> &args) {
  if (function_name == "hostname") {
    return make_dynamic_function_incomplete<expr_hostname>(function_name, args, 0);
  } else if (function_name == "toUpper") {
    return make_dynamic_function_incomplete<expr_toUpper>(function_name, args, 1);
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
  } else {
    std::string msg("Unknown expression function: ");
    msg.append(function_name);
    throw std::runtime_error(msg);
  }
}

Expression make_dynamic_function_postfix(const Expression &subject, const Expression &fn) {
  return fn.complete(subject);
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
    return make_dynamic([val_fn, other_val_fn](const Parameters &params) -> std::string {
      std::string result = val_fn(params);
      result.append(other_val_fn(params));
      return result;
    });
  } else if (isDynamic() && !other_expr.isDynamic()) {
    auto val_fn = val_fn_;
    auto other_val = other_expr.val_;
    return make_dynamic([val_fn, other_val](const Parameters &params) -> std::string {
      std::string result = val_fn(params);
      result.append(other_val);
      return result;
    });
  } else if (!isDynamic() && other_expr.isDynamic()) {
    auto val = val_;
    auto other_val_fn = other_expr.val_fn_;
    return make_dynamic([val, other_val_fn](const Parameters &params) -> std::string {
      std::string result(val);
      result.append(other_val_fn(params));
      return result;
    });
  } else if (!isDynamic() && !other_expr.isDynamic()) {
    std::string result(val_);
    result.append(other_expr.val_);
    return make_static(result);
  } else {
    throw std::runtime_error("Invalid function composition");
  }
}

std::string Expression::operator()(const Parameters &params) const {
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
