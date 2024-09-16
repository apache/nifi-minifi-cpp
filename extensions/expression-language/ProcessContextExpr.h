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

#include <memory>
#include <unordered_map>
#include <string>

#include "core/ProcessContext.h"
#include "impl/expression/Expression.h"

namespace org::apache::nifi::minifi::core {

/**
 * Purpose and Justification: Used to inject EL functionality into the classloader and remove EL as
 * a core functionality. This created linking issues as it was always required even in a disabled
 * state. With this case, we can rely on instantiation of a builder to create the necessary
 * ProcessContext. *
 */
class ProcessContextExpr final : public core::ProcessContextImpl {
 public:
  ProcessContextExpr(Processor& processor, controller::ControllerServiceProvider* controller_service_provider,
                     const std::shared_ptr<core::Repository> &repo, const std::shared_ptr<core::Repository> &flow_repo,
                     const std::shared_ptr<core::ContentRepository> &content_repo = core::repository::createFileSystemRepository())
      : core::ProcessContextImpl(processor, controller_service_provider, repo, flow_repo, content_repo),
        logger_(logging::LoggerFactory<ProcessContextExpr>::getLogger()) {
  }

  ProcessContextExpr(Processor& processor, controller::ControllerServiceProvider* controller_service_provider,
                     const std::shared_ptr<core::Repository> &repo, const std::shared_ptr<core::Repository> &flow_repo, const std::shared_ptr<minifi::Configure> &configuration,
                     const std::shared_ptr<core::ContentRepository> &content_repo = core::repository::createFileSystemRepository())
      : core::ProcessContextImpl(processor, controller_service_provider, repo, flow_repo, configuration, content_repo),
        logger_(logging::LoggerFactory<ProcessContextExpr>::getLogger()) {
  }

  ~ProcessContextExpr() override = default;

  nonstd::expected<std::string, std::error_code> getProperty(std::string_view name, const FlowFile*) const override;
  nonstd::expected<std::string, std::error_code> getDynamicProperty(std::string_view name, const FlowFile*) const override;

  nonstd::expected<void, std::error_code> setProperty(std::string_view name, std::string value) override;
  nonstd::expected<void, std::error_code> setDynamicProperty(std::string name, std::string value) override;

  std::map<std::string, std::string> getDynamicProperties(const FlowFile*) const override;

 private:
  mutable std::unordered_map<std::string, expression::Expression, utils::string::transparent_string_hash, std::equal_to<>> cached_expressions_;
  mutable std::unordered_map<std::string, expression::Expression, utils::string::transparent_string_hash, std::equal_to<>> cached_dynamic_expressions_;

  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core
