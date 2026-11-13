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
