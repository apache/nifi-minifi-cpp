// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <system_error>
#include <string_view>
#include "utils/expected.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/StateManager.h"
#include "minifi-cpp/core/controller/ControllerServiceHandle.h"
#include "minifi-cpp/provenance/ProvenanceRepository.h"
#include "minifi-cpp/utils/Id.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi {
class Configure;
}  // namespace org::apache::nifi::minifi

namespace org::apache::nifi::minifi::core::reporting {

class ReportingTaskContext {
 public:
  virtual ~ReportingTaskContext() = default;

  [[nodiscard]]
  virtual std::expected<std::string, std::error_code> getProperty(std::string_view name) const = 0;

  [[nodiscard]]
  std::expected<std::string, std::error_code> getProperty(const Property& property) const {
    return getProperty(property.getName());
  }

  [[nodiscard]]
  std::expected<std::string, std::error_code> getProperty(const PropertyReference& property_reference) const {
    return getProperty(property_reference.name);
  }

  [[nodiscard]]
  virtual std::shared_ptr<core::controller::ControllerServiceHandle> getControllerService(const std::string& service_name, const utils::Identifier& reporting_task_uuid) const = 0;

  [[nodiscard]]
  virtual std::shared_ptr<provenance::ProvenanceRepository> getProvenanceRepository() = 0;

  [[nodiscard]]
  virtual StateManager* getStateManager() = 0;

  [[nodiscard]]
  virtual uint8_t getMaxConcurrentTasks() const = 0;

  virtual void yield() = 0;

  // The returned configuration is valid during onSchedule and onTrigger.
  [[nodiscard]]
  virtual gsl::not_null<Configure*> getConfiguration() const = 0;
};

}  // namespace org::apache::nifi::minifi::core::reporting
