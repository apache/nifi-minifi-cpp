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

MinifiStatus ControllerServiceImpl::enable(ControllerServiceContext& ctx) {
  try {
    return enableImpl(ctx);
  } catch (const std::exception& e) {
    logger_->log_error("{}", e.what());
    throw;
  }
}

void ControllerServiceImpl::notifyStop() {
  try {
    notifyStopImpl();
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

utils::SmallString<36> ControllerServiceImpl::getUUIDStr() const {
  return getUUID().to_string();
}

}  // namespace org::apache::nifi::minifi::api::core
