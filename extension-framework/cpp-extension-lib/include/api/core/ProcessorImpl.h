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

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "minifi-cpp/core/DynamicPropertyDefinition.h"
#include "minifi-cpp/utils/Id.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"
#include "minifi-cpp/core/ProcessorMetadata.h"
#include "FlowFile.h"
#include "PublishedMetrics.h"
#include "logging/Logger.h"
#include "utils/SmallString.h"

namespace org::apache::nifi::minifi::api {

class Connection;

namespace core {

class ProcessContext;
class ProcessSession;
class ProcessSessionFactory;

class ProcessorImpl {
 public:
  explicit ProcessorImpl(minifi::core::ProcessorMetadata metadata);

  ProcessorImpl(const ProcessorImpl&) = delete;
  ProcessorImpl(ProcessorImpl&&) = delete;
  ProcessorImpl& operator=(const ProcessorImpl&) = delete;
  ProcessorImpl& operator=(ProcessorImpl&&) = delete;

  virtual ~ProcessorImpl();

  minifi_status onTrigger(ProcessContext&, ProcessSession&);

  minifi_status onSchedule(ProcessContext&);

  virtual void onUnSchedule() {}

  static constexpr auto DynamicProperties = std::array<minifi::core::DynamicPropertyDefinition, 0>{};

  static constexpr auto OutputAttributes = std::array<minifi::core::OutputAttributeReference, 0>{};

  std::string getName() const;
  minifi::utils::Identifier getUUID() const;
  minifi::utils::SmallString<36> getUUIDStr() const;

 protected:
  virtual minifi_status onTriggerImpl(ProcessContext&, ProcessSession&) {return MINIFI_STATUS_SUCCESS;}

  virtual minifi_status onScheduleImpl(ProcessContext&) {return MINIFI_STATUS_SUCCESS;}

  minifi::core::ProcessorMetadata metadata_;

  std::shared_ptr<minifi::core::logging::Logger> logger_;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi::api
