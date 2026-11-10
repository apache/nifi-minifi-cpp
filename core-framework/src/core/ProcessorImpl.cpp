// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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

utils::SmallString<utils::Identifier::UUID_STR_LEN> ProcessorImpl::getUUIDStr() const {
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
