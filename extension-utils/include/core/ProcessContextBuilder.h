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
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <memory>
#include "minifi-cpp/core/Property.h"
#include "core/Core.h"
#include "utils/Id.h"
#include "core/ContentRepository.h"
#include "core/logging/LoggerFactory.h"
#include "ProcessContext.h"
#include "core/FlowFile.h"
#include "VariableRegistry.h"
#include "minifi-cpp/core/ProcessContextBuilder.h"

namespace org::apache::nifi::minifi::core {
/**
 * Could use instantiate<T> from core, which uses a simple compile time check to figure out if a destructor is defined
 * and thus that will allow us to know if the context instance exists, but I like using the build because it allows us
 * to eventually share the builder across different contexts and shares up the construction ever so slightly.
 *
 * While this incurs a tiny cost to look up, it allows us to have a replaceable builder that erases the type we are
 * constructing.
 */
class ProcessContextBuilderImpl : public core::CoreComponentImpl, public virtual ProcessContextBuilder, public std::enable_shared_from_this<ProcessContextBuilder> {
 public:
  ProcessContextBuilderImpl(std::string_view name, const minifi::utils::Identifier &uuid);

  explicit ProcessContextBuilderImpl(std::string_view name);

  ~ProcessContextBuilderImpl() override = default;

  MINIFIAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  MINIFIAPI static constexpr bool SupportsDynamicRelationships = false;

  std::shared_ptr<ProcessContextBuilder> withProvider(core::controller::ControllerServiceProvider* controller_service_provider) override;

  std::shared_ptr<ProcessContextBuilder> withProvenanceRepository(const std::shared_ptr<core::Repository> &repo) override;

  std::shared_ptr<ProcessContextBuilder> withFlowFileRepository(const std::shared_ptr<core::Repository> &repo) override;

  std::shared_ptr<ProcessContextBuilder> withContentRepository(const std::shared_ptr<core::ContentRepository> &repo) override;

  std::shared_ptr<ProcessContextBuilder> withConfiguration(const std::shared_ptr<minifi::Configure> &configuration) override;

  std::shared_ptr<core::ProcessContext> build(const std::shared_ptr<ProcessorNode> &processor) override;

 protected:
  std::shared_ptr<minifi::Configure> configuration_;
  core::controller::ControllerServiceProvider* controller_service_provider_ = nullptr;
  std::shared_ptr<core::Repository> prov_repo_;
  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::ContentRepository> content_repo_;
};

}  // namespace org::apache::nifi::minifi::core
