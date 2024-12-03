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
#include <memory>
#include <utility>

#include "core/ProcessContextBuilder.h"

namespace org::apache::nifi::minifi::core::expressions {

/**
 *   Purpose: Creates a context builder that can be used by the class loader to inject EL functionality
 *
 *   Justification: Linking became problematic across platforms since EL was used as a carrier of what was
 *   effectively core functionality. To eliminate this awkward linking and help with disabling EL entirely
 *   on some platforms, this builder was placed into the class loader.
 */
class ExpressionContextBuilder : public core::ProcessContextBuilderImpl {
 public:
  ExpressionContextBuilder(std::string_view name, const minifi::utils::Identifier &uuid);

  explicit ExpressionContextBuilder(std::string_view name);

  ~ExpressionContextBuilder() override;

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  std::shared_ptr<core::ProcessContext> build(const std::shared_ptr<ProcessorNode> &processor) override;
};

}  // namespace org::apache::nifi::minifi::core::expressions
