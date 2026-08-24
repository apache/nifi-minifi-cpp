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
  ReportingTask(std::unique_ptr<ReportingTaskApi> impl);
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
