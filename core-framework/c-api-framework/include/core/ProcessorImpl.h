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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/Annotation.h"
#include "minifi-cpp/core/DynamicProperty.h"
// #include "minifi-cpp/core/ProcessorMetrics.h"
#include "utils/gsl.h"
#include "utils/Id.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"
#include "core/ProcessorMetadata.h"
#include "core/ProcessorDescriptor.h"
#include "core/FlowFile.h"
#include "utils/StringUtils.h"

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

class ProcessorImpl {
 public:
  explicit ProcessorImpl(ProcessorMetadata metadata);

  ProcessorImpl(const ProcessorImpl&) = delete;
  ProcessorImpl(ProcessorImpl&&) = delete;
  ProcessorImpl& operator=(const ProcessorImpl&) = delete;
  ProcessorImpl& operator=(ProcessorImpl&&) = delete;

  virtual ~ProcessorImpl();

  virtual bool isSingleThreaded() const = 0;

  [[nodiscard]]
  virtual bool supportsDynamicProperties() const = 0;

  [[nodiscard]]
  virtual bool supportsDynamicRelationships() const = 0;

  virtual std::string getProcessorType() const = 0;

  void setTriggerWhenEmpty(bool trigger_when_empty) {
    trigger_when_empty_ = trigger_when_empty;
  }

  virtual bool getTriggerWhenEmpty() const {
    return trigger_when_empty_;
  }

  void initialize(ProcessorDescriptor& self);

  void setSupportedRelationships(std::span<const RelationshipDefinition> relationships);

  void setSupportedProperties(std::span<const PropertyReference> properties);

  virtual void initialize() {}

  virtual void onTrigger(ProcessContext&, ProcessSession&) {}

  virtual void onSchedule(ProcessContext&, ProcessSessionFactory&) {}

  // Hook executed when onSchedule fails (throws). Configuration should be reset in this
  virtual void onUnSchedule() {
    notifyStop();
  }

  // Check all incoming connections for work
  virtual bool isWorkAvailable();

  virtual annotation::Input getInputRequirement() const = 0;

  // virtual gsl::not_null<std::shared_ptr<ProcessorMetrics>> getMetrics() const {
  //   return metrics_;
  // }

  static constexpr auto DynamicProperties = std::array<DynamicProperty, 0>{};

  static constexpr auto OutputAttributes = std::array<OutputAttributeReference, 0>{};

  virtual void restore(const std::shared_ptr<FlowFile>& file);

  virtual void forEachLogger(const std::function<void(std::shared_ptr<logging::Logger>)>& callback);

  std::string getName() const;
  utils::Identifier getUUID() const;
  utils::SmallString<36> getUUIDStr() const;

  virtual void notifyStop() {}

 protected:
  ProcessorMetadata metadata_;

  std::atomic<bool> trigger_when_empty_;

  // gsl::not_null<std::shared_ptr<ProcessorMetrics>> metrics_;

  std::shared_ptr<logging::Logger> logger_;

 private:
  mutable std::mutex mutex_;

  ProcessorDescriptor* descriptor_{nullptr};
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
