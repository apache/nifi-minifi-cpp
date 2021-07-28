/**
 * @file ProcessGroup.cpp
 * ProcessGroup class implementation
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
#include "core/ProcessContextBuilder.h"
#include <time.h>
#include <vector>
#include <memory>
#include <string>
#include <queue>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

ProcessContextBuilder::ProcessContextBuilder(const std::string &name, const minifi::utils::Identifier &uuid)
    : core::CoreComponent(name, uuid) {
  content_repo_ = std::make_shared<core::repository::FileSystemRepository>();
  configuration_ = std::make_shared<minifi::Configure>();
}

ProcessContextBuilder::ProcessContextBuilder(const std::string &name)
    : core::CoreComponent(name) {
  content_repo_ = std::make_shared<core::repository::FileSystemRepository>();
  configuration_ = std::make_shared<minifi::Configure>();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilder::withProvider(core::controller::ControllerServiceProvider* controller_service_provider) {
  controller_service_provider_ = controller_service_provider;
  return this->shared_from_this();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilder::withProvenanceRepository(const std::shared_ptr<core::Repository> &repo) {
  prov_repo_ = repo;
  return this->shared_from_this();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilder::withFlowFileRepository(const std::shared_ptr<core::Repository> &repo) {
  flow_repo_ = repo;
  return this->shared_from_this();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilder::withContentRepository(const std::shared_ptr<core::ContentRepository> &repo) {
  content_repo_ = repo;
  return this->shared_from_this();
}

std::shared_ptr<ProcessContextBuilder> ProcessContextBuilder::withConfiguration(const std::shared_ptr<minifi::Configure> &configuration) {
  configuration_ = configuration;
  return this->shared_from_this();
}

std::shared_ptr<core::ProcessContext> ProcessContextBuilder::build(const std::shared_ptr<ProcessorNode> &processor) {
  return std::make_shared<core::ProcessContext>(processor, controller_service_provider_, prov_repo_, flow_repo_, configuration_, content_repo_);
}

REGISTER_INTERNAL_RESOURCE(ProcessContextBuilder);

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
