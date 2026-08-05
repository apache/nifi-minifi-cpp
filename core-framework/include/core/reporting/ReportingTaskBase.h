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
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "minifi-cpp/properties/Configure.h"
#include "core/Core.h"
#include "core/ConfigurableComponentImpl.h"
#include "core/Connectable.h"
#include "minifi-cpp/core/reporting/ReportingTaskApi.h"
#include "minifi-cpp/core/reporting/ReportingTaskDescriptor.h"
#include "minifi-cpp/core/reporting/ReportingTaskMetadata.h"

namespace org::apache::nifi::minifi::core::reporting {

class ReportingTaskBase : public ReportingTaskApi {
 public:
  explicit ReportingTaskBase(ReportingTaskMetadata metadata)
      : name_(std::move(metadata.name)),
        uuid_(metadata.uuid),
        logger_(std::move(metadata.logger)) {}

  virtual void initialize() = 0;

  void initialize(ReportingTaskDescriptor& descriptor) final {
    gsl_Expects(!descriptor_);
    descriptor_ = &descriptor;
    auto guard = gsl::finally([&] {descriptor_ = nullptr;});
    initialize();
  }

  void setSupportedProperties(std::span<const PropertyReference> properties) {
    gsl_Expects(descriptor_);
    descriptor_->setSupportedProperties(properties);
  }

  ReportingTaskBase(const ReportingTaskBase&) = delete;
  ReportingTaskBase(ReportingTaskBase&&) = delete;
  ReportingTaskBase& operator=(const ReportingTaskBase&) = delete;
  ReportingTaskBase& operator=(ReportingTaskBase&&) = delete;

  ~ReportingTaskBase() noexcept override = default;

  void onUnSchedule() override {}

 protected:
  std::string name_;
  utils::Identifier uuid_;
  // valid during initialize, sink for supported properties
  ReportingTaskDescriptor* descriptor_{nullptr};

  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::reporting
