/**
 * @file SiteToSiteProvenanceReportingTask.h
 * SiteToSiteProvenanceReportingTask class declaration
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

#include <memory>
#include <mutex>
#include <stack>
#include <utility>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "RemoteProcessGroupPort.h"
#include "core/logging/LoggerFactory.h"
#include "core/reporting/ReportingTaskBase.h"

namespace org::apache::nifi::minifi::core::reporting {

class SiteToSiteProvenanceReportingTask : public ReportingTaskBase {
 public:
  explicit SiteToSiteProvenanceReportingTask(ReportingTaskMetadata metadata)
      : ReportingTaskBase{metadata},
        remote_port_{metadata.name, metadata.uuid, std::make_unique<RemoteProcessGroupPort>(metadata.name, "", Configure::create(),
        metadata.uuid, sitetosite::TransferDirection::SEND, metadata.logger)}
  {
    batch_size_ = 100;
  }

  ~SiteToSiteProvenanceReportingTask() override = default;

  static const char *ProvenanceAppStr;

  static std::string getJsonReport(const std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records);

  void onSchedule(ReportingTaskContext& context) override;
  void onTrigger(ReportingTaskContext& context) override;

  void initialize() override;

  void setBatchSize(int size) {
    batch_size_ = size;
  }

  int getBatchSize() const {
    return (batch_size_);
  }

 private:
  Processor remote_port_;
  int batch_size_;
};

// SiteToSiteProvenanceReportingTask

}  // namespace org::apache::nifi::minifi::core::reporting
