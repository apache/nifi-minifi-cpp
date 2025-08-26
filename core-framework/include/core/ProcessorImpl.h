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

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>

#include "core/ConfigurableComponentImpl.h"
#include "minifi-cpp/core/Property.h"
#include "core/Core.h"
#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/DynamicProperty.h"
#include "minifi-cpp/core/Scheduling.h"
#include "minifi-cpp/core/ProcessorMetricsExtension.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/Id.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"
#include "minifi-cpp/core/ProcessorApi.h"
#include "utils/PropertyErrors.h"
#include "minifi-cpp/core/ProcessorMetadata.h"
#include "minifi-cpp/Exception.h"

#define ADD_GET_PROCESSOR_NAME \
  std::string getProcessorType() const override { \
    auto class_name = org::apache::nifi::minifi::core::className<decltype(*this)>(); \
    auto splitted = org::apache::nifi::minifi::utils::string::split(class_name, "::"); \
    return splitted[splitted.size() - 1]; \
  }

#define ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS \
  bool supportsDynamicProperties() const override { return SupportsDynamicProperties; } \
  bool supportsDynamicRelationships() const override { return SupportsDynamicRelationships; } \
  minifi::core::annotation::Input getInputRequirement() const override { return InputRequirement; } \
  bool isSingleThreaded() const override { return IsSingleThreaded; } \
  ADD_GET_PROCESSOR_NAME

namespace org::apache::nifi::minifi {

class Connection;

namespace core {

class ProcessContext;
class ProcessSession;
class ProcessSessionFactory;

#define BUILDING_DLL 1

class ProcessorImpl : public virtual ProcessorApi {
 public:
  explicit ProcessorImpl(ProcessorMetadata metadata);

  ProcessorImpl(const ProcessorImpl&) = delete;
  ProcessorImpl(ProcessorImpl&&) = delete;
  ProcessorImpl& operator=(const ProcessorImpl&) = delete;
  ProcessorImpl& operator=(ProcessorImpl&&) = delete;

  ~ProcessorImpl() override;

  bool isSingleThreaded() const override = 0;

  [[nodiscard]] bool supportsDynamicProperties() const override = 0;

  [[nodiscard]] bool supportsDynamicRelationships() const override = 0;

  std::string getProcessorType() const override = 0;

  void setTriggerWhenEmpty(bool trigger_when_empty) {
    trigger_when_empty_ = trigger_when_empty;
  }

  bool getTriggerWhenEmpty() const override {
    return trigger_when_empty_;
  }

  void initialize(ProcessorDescriptor& self) final;

  void setSupportedRelationships(std::span<const RelationshipDefinition> relationships);

  void setSupportedProperties(std::span<const PropertyReference> properties);
  void setSupportedProperties(std::span<const Property> properties);

  virtual void initialize() {}

  void onTrigger(ProcessContext&, ProcessSession&) override {}

  void onSchedule(ProcessContext&, ProcessSessionFactory&) override {}

  // Hook executed when onSchedule fails (throws). Configuration should be reset in this
  void onUnSchedule() override {
    notifyStop();
  }

  // Check all incoming connections for work
  bool isWorkAvailable() override;

  annotation::Input getInputRequirement() const override = 0;

  std::shared_ptr<ProcessorMetricsExtension> getMetricsExtension() const override {
    return metrics_extension_;
  }

  static constexpr auto DynamicProperties = std::array<DynamicProperty, 0>{};

  static constexpr auto OutputAttributes = std::array<OutputAttributeReference, 0>{};

  void restore(const std::shared_ptr<FlowFile>& file) override;

  void forEachLogger(const std::function<void(std::shared_ptr<logging::Logger>)>& callback) override;

  std::string getName() const;
  utils::Identifier getUUID() const;
  utils::SmallString<36> getUUIDStr() const;

 protected:
  void notifyStop() override {
  }

  ProcessorMetadata metadata_;

  std::atomic<bool> trigger_when_empty_;

  std::shared_ptr<ProcessorMetricsExtension> metrics_extension_;

  std::shared_ptr<logging::Logger> logger_;

 private:
  mutable std::mutex mutex_;

  ProcessorDescriptor* descriptor_{nullptr};
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
