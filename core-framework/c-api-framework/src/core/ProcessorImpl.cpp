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
#include "api/core/ProcessorImpl.h"

#include <ctime>
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
      trigger_when_empty_(false),
      // metrics_(std::make_shared<ProcessorMetricsImpl>(*this)),
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
  gsl_Assert("Not implemented");
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

void ProcessorImpl::onSchedule(ProcessContext& ctx) {
  try {
    onScheduleImpl(ctx);
  } catch (const std::exception& e) {
    logger_->log_error("{}", e.what());
    throw;
  } catch (...) {
    throw;
  }
}

void ProcessorImpl::onTrigger(ProcessContext& ctx, ProcessSession& session) {
  try {
    onTriggerImpl(ctx, session);
  } catch (const std::exception& e) {
    logger_->log_error("{}", e.what());
    throw;
  } catch (...) {
    throw;
  }
}


}  // namespace org::apache::nifi::minifi::api::core
