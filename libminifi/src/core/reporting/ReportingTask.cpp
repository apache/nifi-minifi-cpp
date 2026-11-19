// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/reporting/ReportingTask.h"
#include "core/reporting/ReportingTaskContextAdapter.h"


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

void ReportingTask::onTrigger(ProcessContext& context, ProcessSession&) {
  ReportingTaskContextAdapter reporting_task_context{context};
  impl_->onTrigger(reporting_task_context);
}

void ReportingTask::onSchedule(ProcessContext& context, ProcessSessionFactory&) {
  ReportingTaskContextAdapter reporting_task_context{context};
  impl_->onSchedule(reporting_task_context);
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
  impl_->forEachLogger(callback);
}

}  // namespace org::apache::nifi::minifi::core::reporting
