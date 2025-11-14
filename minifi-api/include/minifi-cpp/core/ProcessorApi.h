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

#include <memory>
#include <string>

#include "ConfigurableComponent.h"
#include "Property.h"
#include "minifi-cpp/core/DynamicProperty.h"
#include "Core.h"
#include "minifi-cpp/core/Annotation.h"
#include "Scheduling.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "minifi-cpp/utils/gsl.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/core/ProcessorMetricsExtension.h"

namespace org::apache::nifi::minifi {

class Connection;

namespace core {

class ProcessContext;
class ProcessSession;
class ProcessSessionFactory;
class ProcessorDescriptor;

class ProcessorApi {
 public:
  virtual ~ProcessorApi() = default;

  virtual bool isWorkAvailable() = 0;

  virtual void restore(const std::shared_ptr<FlowFile>& file) = 0;


  [[nodiscard]] virtual bool supportsDynamicProperties() const = 0;
  [[nodiscard]] virtual bool supportsDynamicRelationships() const = 0;

  virtual void initialize(ProcessorDescriptor& descriptor) = 0;
  virtual bool isSingleThreaded() const = 0;
  virtual std::string getProcessorType() const = 0;
  virtual bool getTriggerWhenEmpty() const = 0;
  virtual void onTrigger(ProcessContext&, ProcessSession&) = 0;
  virtual void onSchedule(ProcessContext&, ProcessSessionFactory&) = 0;
  virtual void onUnSchedule() = 0;
  virtual void notifyStop() = 0;
  virtual annotation::Input getInputRequirement() const = 0;
  virtual std::shared_ptr<ProcessorMetricsExtension> getMetricsExtension() const = 0;
  virtual void forEachLogger(const std::function<void(std::shared_ptr<logging::Logger>)>& callback) = 0;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
