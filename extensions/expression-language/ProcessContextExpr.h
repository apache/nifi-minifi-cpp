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
  ProcessContextExpr(const std::shared_ptr<ProcessorNode> &processor, controller::ControllerServiceProvider* controller_service_provider,
                     const std::shared_ptr<core::Repository> &repo, const std::shared_ptr<core::Repository> &flow_repo,
                     const std::shared_ptr<core::ContentRepository> &content_repo = core::repository::createFileSystemRepository())
      : core::ProcessContextImpl(processor, controller_service_provider, repo, flow_repo, content_repo),
        logger_(logging::LoggerFactory<ProcessContextExpr>::getLogger()) {
  }

  ProcessContextExpr(const std::shared_ptr<ProcessorNode> &processor, controller::ControllerServiceProvider* controller_service_provider,
                     const std::shared_ptr<core::Repository> &repo, const std::shared_ptr<core::Repository> &flow_repo, const std::shared_ptr<minifi::Configure> &configuration,
                     const std::shared_ptr<core::ContentRepository> &content_repo = core::repository::createFileSystemRepository())
      : core::ProcessContextImpl(processor, controller_service_provider, repo, flow_repo, configuration, content_repo),
        logger_(logging::LoggerFactory<ProcessContextExpr>::getLogger()) {
  }

  ~ProcessContextExpr() override = default;

  bool getProperty(const Property& property, std::string &value, const FlowFile* const flow_file) override;

  bool getProperty(const PropertyReference& property, std::string &value, const FlowFile* const flow_file) override;

  bool getDynamicProperty(const Property &property, std::string &value, const FlowFile* const flow_file) override;

  bool setProperty(const std::string& property, std::string value) override;

  bool setDynamicProperty(const std::string& property, std::string value) override;

 private:
  bool getProperty(bool supports_expression_language, std::string_view property_name, std::string& value, const FlowFile* const flow_file);

  std::unordered_map<std::string, org::apache::nifi::minifi::expression::Expression> expressions_;
  std::unordered_map<std::string, org::apache::nifi::minifi::expression::Expression> dynamic_property_expressions_;
  std::unordered_map<std::string, std::string> expression_strs_;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core
