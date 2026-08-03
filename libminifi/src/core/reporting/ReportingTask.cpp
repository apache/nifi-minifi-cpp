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

#include "core/reporting/ReportingTask.h"


namespace org::apache::nifi::minifi::core::reporting {

namespace {

class ReportingTaskDescriptorWrapper : public ReportingTaskDescriptor {
 public:
  explicit ReportingTaskDescriptorWrapper(ProcessorDescriptor& descriptor): descriptor_(descriptor) {}

  void setSupportedProperties(std::span<const PropertyReference> properties) override {
    descriptor_.setSupportedProperties(properties);
  }

  void setSupportedProperties(std::span<const Property> properties) override {
    descriptor_.setSupportedProperties(properties);
  }

 private:
  ProcessorDescriptor& descriptor_;
};

}  // namespace

ReportingTask::ReportingTask(std::unique_ptr<ReportingTaskApi> impl): impl_(std::move(impl)) {}

bool ReportingTask::isWorkAvailable() {
  return false;
}

void ReportingTask::restore(const std::shared_ptr<FlowFile>&) {
  throw std::runtime_error("Not supported");
}

[[nodiscard]] bool ReportingTask::supportsDynamicProperties() const {
  return false;
}

[[nodiscard]] bool ReportingTask::supportsDynamicRelationships() const {
  return false;
}

void ReportingTask::initialize(ProcessorDescriptor& descriptor) {
  ReportingTaskDescriptorWrapper reporting_descriptor(descriptor);
  impl_->initialize(reporting_descriptor);
}

bool ReportingTask::isSingleThreaded() const {
  return true;
}

std::string ReportingTask::getProcessorType() const {
  return "ReportingTask";
}

void ReportingTask::onTrigger(ProcessContext& context, ProcessSession&) {
  impl_->onTrigger(context);
}

void ReportingTask::onSchedule(ProcessContext& context, ProcessSessionFactory&) {
  impl_->onSchedule(context);
  context.setTriggerWhenEmpty(true);
}

void ReportingTask::onUnSchedule() {
  notifyStop();
}

void ReportingTask::notifyStop() {
  impl_->onUnSchedule();
}

annotation::Input ReportingTask::getInputRequirement() const {
  return annotation::Input::INPUT_FORBIDDEN;
}

std::shared_ptr<ProcessorMetricsExtension> ReportingTask::getMetricsExtension() const {
  return nullptr;
}

void ReportingTask::forEachLogger(const std::function<void(std::shared_ptr<logging::Logger>)>&) {
  // pass
}

}  // namespace org::apache::nifi::minifi::core::reporting
