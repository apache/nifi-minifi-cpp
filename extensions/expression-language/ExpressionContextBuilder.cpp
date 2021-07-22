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

#include "ExpressionContextBuilder.h"

#include <memory>
#include <string>

#include "ProcessContextExpr.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace expressions {

ExpressionContextBuilder::ExpressionContextBuilder(const std::string &name, const minifi::utils::Identifier &uuid)
    : core::ProcessContextBuilder(name, uuid) {
}

ExpressionContextBuilder::ExpressionContextBuilder(const std::string &name)
    : core::ProcessContextBuilder(name) {
}

ExpressionContextBuilder::~ExpressionContextBuilder() = default;

std::shared_ptr<core::ProcessContext> ExpressionContextBuilder::build(const std::shared_ptr<ProcessorNode> &processor) {
  return std::make_shared<core::ProcessContextExpr>(processor, controller_service_provider_, prov_repo_, flow_repo_, configuration_, content_repo_);
}

REGISTER_INTERNAL_RESOURCE_AS(ExpressionContextBuilder, ("ProcessContextBuilder"));

} /* namespace expressions */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
