// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "PushGrafanaLoki.h"

#include "http/HTTPClient.h"
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

class PushGrafanaLokiREST : public PushGrafanaLoki {
 public:
  EXTENSIONAPI static constexpr const char* Description = "A Grafana Loki push processor that uses the Grafana Loki REST API. The processor expects each flow file to contain a single log line to be "
                                                          "pushed to Grafana Loki, therefore it is usually used together with the TailFile processor.";

  using PushGrafanaLoki::PushGrafanaLoki;
  ~PushGrafanaLokiREST() override = default;

  EXTENSIONAPI static constexpr auto ReadTimeout = core::PropertyDefinitionBuilder<>::createProperty("Read Timeout")
    .withDescription("Max wait time for response from remote service.")
    .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
    .withDefaultValue("15 s")
    .isRequired(true)
    .build();
  EXTENSIONAPI static constexpr auto Username = core::PropertyDefinitionBuilder<>::createProperty("Username")
    .withDescription("Username for authenticating using basic authentication.")
    .build();
  EXTENSIONAPI static constexpr auto Password = core::PropertyDefinitionBuilder<>::createProperty("Password")
    .withDescription("Password for authenticating using basic authentication.")
    .isSensitive(true)
    .build();
  EXTENSIONAPI static constexpr auto BearerTokenFile = core::PropertyDefinitionBuilder<>::createProperty("Bearer Token File")
    .withDescription("Path of file containing bearer token for bearer token authentication.")
    .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(PushGrafanaLoki::Properties, std::to_array<core::PropertyReference>({
      ReadTimeout,
      Username,
      Password,
      BearerTokenFile
  }));

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  std::string createLokiJson(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) const;
  std::expected<void, std::string> submitRequest(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) override;
  void initializeHttpClient(core::ProcessContext& context);
  void setUpStreamLabels(core::ProcessContext& context) override;
  void setupClientTimeouts(const core::ProcessContext& context);
  void setAuthorization(const core::ProcessContext& context);
  void addLogLineMetadata(rapidjson::Value& log_line, rapidjson::Document::AllocatorType& allocator, core::FlowFile& flow_file) const;

  std::map<std::string, std::string> stream_label_attributes_;
  http::HTTPClient client_;
};

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
