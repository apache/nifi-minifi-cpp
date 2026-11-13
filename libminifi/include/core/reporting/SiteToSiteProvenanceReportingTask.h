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
        remote_port_{"RemoteProcessGroupPort", metadata.name, metadata.uuid, std::make_unique<RemoteProcessGroupPort>(metadata.name, "",
        metadata.uuid, sitetosite::TransferDirection::SEND, metadata.logger)} {}

  ~SiteToSiteProvenanceReportingTask() override = default;

  static const char *ProvenanceAppStr;

  MINIFIAPI static constexpr const char* Description = "Publishes Provenance events using the Site To Site protocol. "
      "The events are read from the agent's provenance repository and are sent in JSON format to the remote input port of a NiFi instance.";

  MINIFIAPI static constexpr auto DestinationUrl =
      core::PropertyDefinitionBuilder<>::createProperty("Destination URL")
          .withDescription("The URL of the destination NiFi instance in the format of http(s)://host:port/nifi. "
                           "This is used to initiate the Site-to-Site connection.")
          .isRequired(true)
          .build();

  MINIFIAPI static constexpr auto BatchSize =
      core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
          .withDescription("The maximum number of provenance events to send to the destination in a single transaction.")
          .isRequired(true)
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("100")
          .build();

  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      DestinationUrl,
      BatchSize,
      RemoteProcessGroupPort::SSLContext,
      RemoteProcessGroupPort::portUUID,
      RemoteProcessGroupPort::idleTimeout});

  static std::string getJsonReport(const std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records);

  void onSchedule(ReportingTaskContext& context) override;
  void onTrigger(ReportingTaskContext& context) override;

  void initialize() override;


 private:
  Processor remote_port_;
  uint64_t batch_size_{100};
};

// SiteToSiteProvenanceReportingTask

}  // namespace org::apache::nifi::minifi::core::reporting
