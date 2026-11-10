// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>

#include "Property.h"
#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/FlowFile.h"
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
