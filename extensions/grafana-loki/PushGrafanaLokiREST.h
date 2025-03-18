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

#include "PushGrafanaLoki.h"

#include "http/HTTPClient.h"
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

class PushGrafanaLokiREST : public PushGrafanaLoki {
 public:
  EXTENSIONAPI static constexpr const char* Description = "A Grafana Loki push processor that uses the Grafana Loki REST API. The processor expects each flow file to contain a single log line to be "
                                                          "pushed to Grafana Loki, therefore it is usually used together with the TailFile processor.";

  explicit PushGrafanaLokiREST(const std::string& name, const utils::Identifier& uuid = {})
      : PushGrafanaLoki(name, uuid, core::logging::LoggerFactory<PushGrafanaLokiREST>::getLogger(uuid)) {
  }
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
  nonstd::expected<void, std::string> submitRequest(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) override;
  void initializeHttpClient(core::ProcessContext& context);
  void setUpStreamLabels(core::ProcessContext& context) override;
  void setupClientTimeouts(const core::ProcessContext& context);
  void setAuthorization(const core::ProcessContext& context);
  void addLogLineMetadata(rapidjson::Value& log_line, rapidjson::Document::AllocatorType& allocator, core::FlowFile& flow_file) const;

  std::map<std::string, std::string> stream_label_attributes_;
  http::HTTPClient client_;
};

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
