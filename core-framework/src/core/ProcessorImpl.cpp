/**
 * @file Processor.cpp
 * Processor class implementation
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
#include "core/ProcessorImpl.h"

#include <ctime>
#include <cctype>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "minifi-cpp/Connection.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/ProcessorConfig.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessSessionFactory.h"
#include "minifi-cpp/utils/gsl.h"
#include "range/v3/algorithm/any_of.hpp"
#include "fmt/format.h"
#include "minifi-cpp/Exception.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core {

ProcessorImpl::ProcessorImpl(ProcessorMetadata metadata)
    : metadata_(std::move(metadata)),
      trigger_when_empty_(false),
      logger_(metadata_.logger) {
  logger_->log_debug("Processor {} created with uuid {}", getName(), getUUIDStr());
}

ProcessorImpl::~ProcessorImpl() {
  logger_->log_debug("Destroying processor {} with uuid {}", getName(), getUUIDStr());
}

bool ProcessorImpl::isWorkAvailable() {
  return false;
}

void ProcessorImpl::restore(const std::shared_ptr<FlowFile>& /*file*/) {
  gsl_Assert(false && "Not implemented");
}

std::string ProcessorImpl::getName() const {
  return metadata_.name;
}

utils::Identifier ProcessorImpl::getUUID() const {
  return metadata_.uuid;
}

utils::SmallString<36> ProcessorImpl::getUUIDStr() const {
  return getUUID().to_string();
}

void ProcessorImpl::initialize(ProcessorDescriptor& self) {
  gsl_Expects(!descriptor_);
  descriptor_ = &self;
  auto guard = gsl::finally([&] {descriptor_ = nullptr;});
  initialize();
}

void ProcessorImpl::setSupportedRelationships(std::span<const RelationshipDefinition> relationships) {
  gsl_Expects(descriptor_);
  descriptor_->setSupportedRelationships(relationships);
}

void ProcessorImpl::setSupportedProperties(std::span<const PropertyReference> properties) {
  gsl_Expects(descriptor_);
  descriptor_->setSupportedProperties(properties);
}

void ProcessorImpl::setSupportedProperties(std::span<const Property> properties) {
  gsl_Expects(descriptor_);
  descriptor_->setSupportedProperties(properties);
}

void ProcessorImpl::forEachLogger(const std::function<void(std::shared_ptr<logging::Logger>)>& callback) {
  callback(logger_);
}

}  // namespace org::apache::nifi::minifi::core
