// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <system_error>

#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/reporting/ReportingTaskContext.h"

namespace org::apache::nifi::minifi::core::reporting {

// Exposes the reporting task relevant subset of a ProcessContext. It only references the
// wrapped context, so it must not outlive it, i.e. it is only valid during the call
// (onSchedule/onTrigger) it was created for.
class ReportingTaskContextAdapter final : public ReportingTaskContext {
 public:
  explicit ReportingTaskContextAdapter(ProcessContext& context) : context_(context) {}

  using ReportingTaskContext::getProperty;

  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(std::string_view name) const override {
    return context_.getProperty(name);
  }

  [[nodiscard]] std::shared_ptr<controller::ControllerServiceHandle> getControllerService(const std::string& service_name,
      const utils::Identifier& reporting_task_uuid) const override {
    return context_.getControllerService(service_name, reporting_task_uuid);
  }

  [[nodiscard]] std::shared_ptr<provenance::ProvenanceRepository> getProvenanceRepository() override {
    return context_.getProvenanceRepository();
  }

  [[nodiscard]] StateManager* getStateManager() override {
    return context_.getStateManager();
  }

  [[nodiscard]] uint8_t getMaxConcurrentTasks() const override {
    return context_.getMaxConcurrentTasks();
  }

  void yield() override {
    context_.yield();
  }

  [[nodiscard]] gsl::not_null<Configure*> getConfiguration() const override {
    return context_.getConfiguration();
  }

 private:
  ProcessContext& context_;
};

}  // namespace org::apache::nifi::minifi::core::reporting
