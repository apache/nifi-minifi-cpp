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

#ifndef EXTENSIONS_EXPRESSION_LANGUAGE_EXPRESSIONCONTEXTBUILDER_H_
#define EXTENSIONS_EXPRESSION_LANGUAGE_EXPRESSIONCONTEXTBUILDER_H_

#include <string>
#include <memory>

#include "core/ProcessContextBuilder.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace expressions {

/**
 *   Purpose: Creates a context builder that can be used by the class loader to inject EL functionality
 *
 *   Justification: Linking became problematic across platforms since EL was used as a carrier of what was
 *   effectively core functionality. To eliminate this awkward linking and help with disabling EL entirely
 *   on some platforms, this builder was placed into the class loader.
 */
class ExpressionContextBuilder : public core::ProcessContextBuilder {
 public:
  ExpressionContextBuilder(const std::string &name, const minifi::utils::Identifier &uuid);

  explicit ExpressionContextBuilder(const std::string &name);

  virtual ~ExpressionContextBuilder();

  std::shared_ptr<core::ProcessContext> build(const std::shared_ptr<ProcessorNode> &processor) override;
};

} /* namespace expressions */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_EXPRESSION_LANGUAGE_EXPRESSIONCONTEXTBUILDER_H_ */
