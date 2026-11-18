// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessorApi.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"
#include "minifi-cpp/core/reporting/ReportingTaskApi.h"
#include "minifi-cpp/core/reporting/ReportingTaskContext.h"
#include "minifi-cpp/core/reporting/ReportingTaskDescriptor.h"

namespace org::apache::nifi::minifi::core::reporting {

class ReportingTask : public ProcessorApi {
 public:
  explicit ReportingTask(std::unique_ptr<ReportingTaskApi> impl);
  ~ReportingTask() override = default;

  bool isWorkAvailable() override;

  void restore(const std::shared_ptr<FlowFile>& file) override;

  [[nodiscard]] bool supportsDynamicProperties() const override;

  [[nodiscard]] bool supportsDynamicRelationships() const override;

  void initialize(ProcessorDescriptor& descriptor) override;

  bool isSingleThreaded() const override;

  void onTrigger(ProcessContext& context, ProcessSession&) override;

  void onSchedule(ProcessContext& context, ProcessSessionFactory&) override;

  void onUnSchedule() override;

  void notifyStop() override;

  annotation::Input getInputRequirement() const override;

  std::shared_ptr<ProcessorMetricsExtension> getMetricsExtension() const override;

  void forEachLogger(const std::function<void(std::shared_ptr<logging::Logger>)>& callback) override;

  ReportingTaskApi& getImpl() const {
    gsl_Assert(impl_);
    return *impl_;
  }

  template<typename T>
  T& getImpl() const {
    auto* res = dynamic_cast<T*>(&getImpl());
    gsl_Assert(res);
    return *res;
  }

 private:
  std::unique_ptr<ReportingTaskApi> impl_;
};

}  // namespace org::apache::nifi::minifi::core::reporting
