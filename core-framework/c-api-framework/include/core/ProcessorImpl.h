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
#include "core/FlowFile.h"
#include "utils/StringUtils.h"

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

  void setTriggerWhenEmpty(bool trigger_when_empty) {
    trigger_when_empty_ = trigger_when_empty;
  }

  virtual bool getTriggerWhenEmpty() const {
    return trigger_when_empty_;
  }

  virtual void onTrigger(ProcessContext&, ProcessSession&) {}

  virtual void onSchedule(ProcessContext&) {}

  virtual void onUnSchedule() {}

  virtual bool isWorkAvailable();

  static constexpr auto DynamicProperties = std::array<DynamicProperty, 0>{};

  static constexpr auto OutputAttributes = std::array<OutputAttributeReference, 0>{};

  virtual void restore(const std::shared_ptr<FlowFile>& file);

  std::string getName() const;
  utils::Identifier getUUID() const;
  utils::SmallString<36> getUUIDStr() const;

 protected:
  ProcessorMetadata metadata_;

  std::atomic<bool> trigger_when_empty_;

  std::shared_ptr<logging::Logger> logger_;

 private:
  mutable std::mutex mutex_;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
