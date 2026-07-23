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

class ReportingTaskContextWrapper : public ReportingTaskContext {
 public:
  explicit ReportingTaskContextWrapper(ProcessContext& context): context_(context) {}
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(std::string_view name) const override {
    return context_.getProperty(name);
  }

 private:
  ProcessContext& context_;
};

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

void ReportingTask::restore(const std::shared_ptr<FlowFile>& file) {
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

}

void ReportingTask::onTrigger(ProcessContext& context, ProcessSession&) {
  ReportingTaskContextWrapper reporting_context{context};
  impl_->onTrigger(reporting_context);
}

void ReportingTask::onSchedule(ProcessContext& context, ProcessSessionFactory&) {
  context.setTriggerWhenEmpty(true);
  ReportingTaskContextWrapper reporting_context{context};
  impl_->onSchedule(reporting_context);
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

void ReportingTask::forEachLogger(const std::function<void(std::shared_ptr<logging::Logger>)>& callback) {
  // pass
}

}  // namespace org::apache::nifi::minifi::core::reporting
