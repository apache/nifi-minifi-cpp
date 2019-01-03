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

#ifndef NIFI_MINIFI_CPP_EXPRESSION_H
#define NIFI_MINIFI_CPP_EXPRESSION_H

#define EXPRESSION_LANGUAGE_USE_REGEX

// Disable regex in EL for incompatible compilers
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
#undef EXPRESSION_LANGUAGE_USE_REGEX
#endif

#define EXPRESSION_LANGUAGE_USE_DATE

// Disable date in EL for incompatible compilers
#if __GNUC__ < 5
#undef EXPRESSION_LANGUAGE_USE_DATE
#endif

#include <Value.h>
#include <core/FlowFile.h>
#include <core/VariableRegistry.h>

#include <string>
#include <memory>
#include <functional>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace expression {

struct Parameters {
  std::weak_ptr<core::FlowFile> flow_file;
  std::weak_ptr<core::VariableRegistry> registry_;
  Parameters(std::shared_ptr<core::VariableRegistry> reg, std::shared_ptr<core::FlowFile> ff = nullptr)
      : registry_(reg) {
    flow_file = ff;
  }

  Parameters(std::shared_ptr<core::FlowFile> ff = nullptr) {
    flow_file = ff;
  }

};

class Expression;

static const std::function<Value(const Parameters &params, const std::vector<Expression> &sub_exprs)> NOOP_FN;

/**
 * A NiFi expression language expression.
 */
class Expression {
 public:

  Expression() {
    val_fn_ = NOOP_FN;
  }

  explicit Expression(Value val, std::function<Value(const Parameters &, const std::vector<Expression> &)> val_fn = NOOP_FN)
      : val_fn_(std::move(val_fn)),
        fn_args_(),
        is_multi_(false) {
    val_ = val;
    sub_expr_generator_ = [](const Parameters &params) -> std::vector<Expression> {return {};};
  }

  /**
   * Whether or not this expression is dynamic. If it is not dynamic, then
   * the expression can be computed at compile time when composed with other
   * expressions.
   *
   * @return true if expression is dynamic
   */
  bool is_dynamic() const;

  /**
   * Whether or not this expression is a multi-expression.
   */
  bool is_multi() const {
    return is_multi_;
  }

  /**
   * Combine this expression with another expression by concatenation. Intermediate results
   * are computed when non-dynamic expressions are composed.
   *
   * @param other_expr
   * @return combined expression
   */
  Expression operator+(const Expression &other_expr) const;

  /**
   * Evaluate the result of the expression as a function of the given parameters.
   *
   * @param params
   * @return dynamically-computed result of expression
   */
  Value operator()(const Parameters &params) const;

  /**
   * Turn this expression into a multi-expression which generates subexpressions dynamically.
   *
   * @param sub_expr_generator function which generates expressions according to given params
   */
  void make_multi(std::function<std::vector<Expression>(const Parameters &params)> sub_expr_generator) {
    this->sub_expr_generator_ = std::move(sub_expr_generator);
    is_multi_ = true;
  }

  /**
   * Composes a multi-expression statically with the given fn.
   *
   * @param fn Value function to compose with
   * @param args function arguments
   * @return composed multi-expression
   */
  Expression compose_multi(const std::function<Value(const std::vector<Value> &)> fn, const std::vector<Expression> &args) const;

  Expression make_aggregate(std::function<Value(const Parameters &params, const std::vector<Expression> &sub_exprs)> val_fn) const;

 protected:
  Value val_;
  std::function<Value(const Parameters &params, const std::vector<Expression> &sub_exprs)> val_fn_;
  std::vector<Expression> fn_args_;
  std::function<std::vector<Expression>(const Parameters &params)> sub_expr_generator_;
  bool is_multi_;
};

/**
 * Compiles an expression from a string in the NiFi expression language syntax.
 *
 * @param expr_str
 * @return
 */
Expression compile(const std::string &expr_str);

/**
 * Creates a string expression that is not dynamic.
 *
 * @param val
 * @return
 */
Expression make_static(std::string val);

/**
 * Creates an arbitrary dynamic expression which evaluates to the given value function.
 *
 * @param val_fn
 * @return
 */
Expression make_dynamic(const std::function<Value(const Parameters &params, const std::vector<Expression> &sub_exprs)> &val_fn);

/**
 * Creates a dynamic expression which evaluates the given flow file attribute.
 *
 * @param attribute_id
 * @return
 */
Expression make_dynamic_attr(const std::string &attribute_id);

/**
 * Creates a dynamic expression which evaluates the given function as defined
 * in NiFi expression language.
 *
 * @param function_name
 * @param args
 * @return
 */
Expression make_dynamic_function(const std::string &function_name, const std::vector<Expression> &args);

/**
 * Creates a chained function composition.
 *
 * @param arg
 * @param chain
 * @return
 */
Expression make_function_composition(const Expression &arg, const std::vector<std::pair<std::string, std::vector<Expression>>> &chain);

} /* namespace expression */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_EXPRESSION_H
