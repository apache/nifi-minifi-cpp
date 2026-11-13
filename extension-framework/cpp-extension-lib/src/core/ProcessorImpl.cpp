// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "api/core/ProcessorImpl.h"

#include <cctype>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "fmt/format.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::api::core {

ProcessorImpl::ProcessorImpl(minifi::core::ProcessorMetadata metadata)
    : metadata_(std::move(metadata)),
      logger_(metadata_.logger) {
  logger_->log_debug("Processor {} created with uuid {}", getName(), getUUIDStr());
}

ProcessorImpl::~ProcessorImpl() {
  logger_->log_debug("Destroying processor {} with uuid {}", getName(), getUUIDStr());
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

minifi_status ProcessorImpl::onSchedule(ProcessContext& ctx) {
  try {
    return onScheduleImpl(ctx);
  } catch (const std::exception& e) {
    logger_->log_error("{}", e.what());
    throw;
  }
}

minifi_status ProcessorImpl::onTrigger(ProcessContext& ctx, ProcessSession& session) {
  try {
    return onTriggerImpl(ctx, session);
  } catch (const std::exception& e) {
    logger_->log_error("{}", e.what());
    throw;
  }
}


}  // namespace org::apache::nifi::minifi::api::core
