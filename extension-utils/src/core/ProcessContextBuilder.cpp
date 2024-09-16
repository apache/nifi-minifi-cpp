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
#include "core/ProcessContextBuilder.h"
#include <memory>
#include <string>
#include "core/logging/LoggerFactory.h"
#include "core/Resource.h"
#include "minifi-cpp/core/repository/FileSystemRepository.h"

namespace org::apache::nifi::minifi::core {

ProcessContextBuilderImpl::ProcessContextBuilderImpl(std::string_view name, const minifi::utils::Identifier &uuid)
    : core::CoreComponentImpl(name, uuid) {
  content_repo_ = repository::createFileSystemRepository();
  configuration_ = minifi::Configure::create();
}

ProcessContextBuilderImpl::ProcessContextBuilderImpl(std::string_view name)
    : core::CoreComponentImpl(name) {
  content_repo_ = repository::createFileSystemRepository();
  configuration_ = minifi::Configure::create();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilderImpl::withProvider(core::controller::ControllerServiceProvider* controller_service_provider) {
  controller_service_provider_ = controller_service_provider;
  return this->shared_from_this();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilderImpl::withProvenanceRepository(const std::shared_ptr<core::Repository> &repo) {
  prov_repo_ = repo;
  return this->shared_from_this();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilderImpl::withFlowFileRepository(const std::shared_ptr<core::Repository> &repo) {
  flow_repo_ = repo;
  return this->shared_from_this();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilderImpl::withContentRepository(const std::shared_ptr<core::ContentRepository> &repo) {
  content_repo_ = repo;
  return this->shared_from_this();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilderImpl::withConfiguration(const std::shared_ptr<minifi::Configure> &configuration) {
  configuration_ = configuration;
  return this->shared_from_this();
}

std::shared_ptr<core::ProcessContext> ProcessContextBuilderImpl::build(const std::shared_ptr<ProcessorNode> &processor) {
  return std::make_shared<core::ProcessContextImpl>(processor, controller_service_provider_, prov_repo_, flow_repo_, configuration_, content_repo_);
}

REGISTER_RESOURCE(ProcessContextBuilderImpl, InternalResource);

}  // namespace org::apache::nifi::minifi::core
