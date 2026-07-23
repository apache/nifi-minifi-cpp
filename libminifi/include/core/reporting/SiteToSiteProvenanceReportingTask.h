// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
