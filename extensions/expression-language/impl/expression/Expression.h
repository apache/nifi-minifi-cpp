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

#include <core/FlowFile.h>

#include <string>
#include <memory>
#include <functional>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace expression {

typedef struct {
  std::weak_ptr<core::FlowFile> flow_file;
} Parameters;

static const std::function<std::string(const Parameters &params)> NOOP_FN;

/**
 * A NiFi expression langauge expression which can be composed with other
 * expressions (via the + operator.
 */
class Expression {
 public:

  explicit Expression(std::string val = "",
                      std::function<std::string(const Parameters &)> val_fn = NOOP_FN)
      : val_(std::move(val)),
        val_fn_(std::move(val_fn)),
        fn_args_() {
  }

  /**
   * Whether or not this expression is dynamic. If it is not dynamic, then
   * the expression can be computed at compile time when composed with other
   * expressions.
   *
   * @return true if expression is dynamic
   */
  bool isDynamic() const;

  /**
   * Combine this expression with another expression. Intermediate results
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
  std::string operator()(const Parameters &params) const;

  /**
   * If this expression is incomplete (i.e. a function with incomplete arguments), then this
   * will return a completed expression based on the provided expression.
   */
  std::function<Expression(Expression)> complete = [](Expression expr) -> Expression {
    throw std::runtime_error("Attempted to complete already complete expression.");
  };

 protected:
  std::string val_;
  std::function<std::string(const Parameters &params)> val_fn_;
  std::vector<Expression> fn_args_;
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
Expression make_dynamic(std::function<std::string(const Parameters &params)> val_fn);

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
 * Creates a dynamic expression which feeds the subject as the first argument into
 * the provided function expression. This enables expressions like attr:toLower()
 *
 * @param subject
 * @param fn
 * @return
 */
Expression make_dynamic_function_postfix(const Expression &subject, const Expression &fn);

} /* namespace expression */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_EXPRESSION_H
