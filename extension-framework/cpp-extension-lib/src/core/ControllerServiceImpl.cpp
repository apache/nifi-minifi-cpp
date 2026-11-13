// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "api/core/ControllerServiceImpl.h"

#include <memory>
#include <string>

#include "fmt/format.h"
namespace org::apache::nifi::minifi::api::core {

ControllerServiceImpl::ControllerServiceImpl(minifi::core::ControllerServiceMetadata metadata)
    : metadata_(std::move(metadata)),
      logger_(metadata_.logger) {
  logger_->log_debug("ControllerService {} created with uuid {}", getName(), getUUIDStr());
}

ControllerServiceImpl::~ControllerServiceImpl() {
  logger_->log_debug("Destroying controller service {} with uuid {}", getName(), getUUIDStr());
}

minifi_status ControllerServiceImpl::enable(ControllerServiceContext& ctx) {
  try {
    return enableImpl(ctx);
  } catch (const std::exception& e) {
    logger_->log_error("{}", e.what());
    throw;
  }
}

void ControllerServiceImpl::disable() {
  try {
    disableImpl();
  } catch (const std::exception& e) {
    logger_->log_error("{}", e.what());
    throw;
  }
}

std::string ControllerServiceImpl::getName() const {
  return metadata_.name;
}

utils::Identifier ControllerServiceImpl::getUUID() const {
  return metadata_.uuid;
}

utils::SmallString<utils::Identifier::UUID_STR_LEN> ControllerServiceImpl::getUUIDStr() const {
  return getUUID().to_string();
}

}  // namespace org::apache::nifi::minifi::api::core
