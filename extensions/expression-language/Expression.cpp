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

#include <expression/Expression.h>
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

template<std::string T(const std::vector<std::string> &)>
Expression make_dynamic_function_incomplete(const std::string &function_name,
                                            const std::vector<Expression> &args,
                                            std::size_t num_args) {
  if (args.size() == num_args) {
    return make_dynamic([=](const Parameters &params) -> std::string {
      std::vector<std::string> evaluated_args;

      for (const auto &arg : args) {
        evaluated_args.emplace_back(arg(params));
      }

      return T(evaluated_args);
    });
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

Expression make_dynamic_function(const std::string &function_name, const std::vector<Expression> &args) {
  if (function_name == "hostname") {
    return make_dynamic_function_incomplete<expr_hostname>(function_name, args, 0);
  } else if (function_name == "toUpper") {
    return make_dynamic_function_incomplete<expr_toUpper>(function_name, args, 1);
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
  } else { // !isDynamic() && !other_expr.isDynamic()
    std::string result(val_);
    result.append(other_expr.val_);
    return make_static(result);
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
