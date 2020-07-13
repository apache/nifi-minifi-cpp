/**
 *
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
#ifndef LIBMINIFI_INCLUDE_CORE_PROCESSCONTEXTBUILDER_H_
#define LIBMINIFI_INCLUDE_CORE_PROCESSCONTEXTBUILDER_H_

#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <memory>
#include "Property.h"
#include "core/Core.h"
#include "utils/Id.h"
#include "core/ContentRepository.h"
#include "properties/Configure.h"
#include "core/repository/FileSystemRepository.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/controller/ControllerServiceLookup.h"
#include "core/logging/LoggerConfiguration.h"
#include "ProcessContext.h"
#include "ProcessorNode.h"
#include "core/Repository.h"
#include "core/FlowFile.h"
#include "VariableRegistry.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
/**
 * Could use instantiate<T> from core, which uses a simple compile time check to figure out if a destructor is defined
 * and thus that will allow us to know if the context instance exists, but I like using the build because it allows us
 * to eventually share the builder across different contexts and shares up the construction ever so slightly.
 *
 * While this incurs a tiny cost to look up, it allows us to have a replaceable builder that erases the type we are
 * constructing.
 */
class ProcessContextBuilder : public core::CoreComponent, public std::enable_shared_from_this<ProcessContextBuilder> {
 public:
  ProcessContextBuilder(const std::string &name, const minifi::utils::Identifier &uuid);

  ProcessContextBuilder(const std::string &name); // NOLINT

  virtual ~ProcessContextBuilder() = default;

  std::shared_ptr<ProcessContextBuilder> withProvider(core::controller::ControllerServiceProvider* controller_service_provider);

  std::shared_ptr<ProcessContextBuilder> withProvenanceRepository(const std::shared_ptr<core::Repository> &repo);

  std::shared_ptr<ProcessContextBuilder> withFlowFileRepository(const std::shared_ptr<core::Repository> &repo);

  std::shared_ptr<ProcessContextBuilder> withContentRepository(const std::shared_ptr<core::ContentRepository> &repo);

  std::shared_ptr<ProcessContextBuilder> withConfiguration(const std::shared_ptr<minifi::Configure> &configuration);

  virtual std::shared_ptr<core::ProcessContext> build(const std::shared_ptr<ProcessorNode> &processor);

 protected:
  std::shared_ptr<minifi::Configure> configuration_;
  core::controller::ControllerServiceProvider* controller_service_provider_;
  std::shared_ptr<core::Repository> prov_repo_;
  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::ContentRepository> content_repo_;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_CORE_PROCESSCONTEXTBUILDER_H_
