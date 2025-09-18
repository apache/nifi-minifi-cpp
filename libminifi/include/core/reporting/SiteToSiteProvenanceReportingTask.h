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
#include "RemoteProcessorGroupPort.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core::reporting {

class SiteToSiteProvenanceReportingTask : public minifi::RemoteProcessorGroupPort {
 public:
  explicit SiteToSiteProvenanceReportingTask(std::shared_ptr<Configure> configure)
      : minifi::RemoteProcessorGroupPort(ReportTaskName, "", std::move(configure),
        utils::IdGenerator::getIdGenerator()->generate(), logging::LoggerFactory<SiteToSiteProvenanceReportingTask>::getLogger()) {
    this->setTriggerWhenEmpty(true);
    batch_size_ = 100;
  }

  ~SiteToSiteProvenanceReportingTask() override = default;

  static constexpr char const* ReportTaskName = "SiteToSiteProvenanceReportingTask";
  static const char *ProvenanceAppStr;

  static void getJsonReport(core::ProcessContext& context, core::ProcessSession& session, std::vector<std::shared_ptr<core::SerializableComponent>> &records, std::string &report); // NOLINT

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void initialize() override;

  void setPortUUID(utils::Identifier &port_uuid) {
    protocol_uuid_ = port_uuid;
  }

  void setBatchSize(int size) {
    batch_size_ = size;
  }

  int getBatchSize() const {
    return (batch_size_);
  }

  void getPortUUID(utils::Identifier & port_uuid) {
    port_uuid = protocol_uuid_;
  }

 private:
  int batch_size_;
};

// SiteToSiteProvenanceReportingTask

}  // namespace org::apache::nifi::minifi::core::reporting
