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

#include "core/logging/LoggerFactory.h"
#include "core/StateManager.h"
#include "grafana-loki-push.grpc.pb.h"
#include "grafana-loki-push.pb.h"
#include "grpc/grpc.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

class PushGrafanaLokiGrpc final : public PushGrafanaLoki {
 public:
  EXTENSIONAPI static constexpr const char* Description = "A Grafana Loki push processor that uses the Grafana Loki Grpc port. The processor expects each flow file to contain a single log line to be "
                                                          "pushed to Grafana Loki, therefore it is usually used together with the TailFile processor.";

  explicit PushGrafanaLokiGrpc(const std::string_view name, const utils::Identifier& uuid = {})
      : PushGrafanaLoki(name, uuid, core::logging::LoggerFactory<PushGrafanaLokiGrpc>::getLogger(uuid)) {
  }
  ~PushGrafanaLokiGrpc() override = default;

  EXTENSIONAPI static constexpr auto KeepAliveTime = core::PropertyDefinitionBuilder<>::createProperty("Keep Alive Time")
    .withDescription("The period after which a keepalive ping is sent on the transport. If not set, then the keep alive is disabled.")
    .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
    .build();
  EXTENSIONAPI static constexpr auto KeepAliveTimeout = core::PropertyDefinitionBuilder<>::createProperty("Keep Alive Timeout")
    .withDescription("The amount of time the sender of the keepalive ping waits for an acknowledgement. If it does not receive an acknowledgment within this time, "
                     "it will close the connection. If not set, then the default value 20 seconds is used.")
    .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
    .build();
  EXTENSIONAPI static constexpr auto MaxPingsWithoutData = core::PropertyDefinitionBuilder<>::createProperty("Max Pings Without Data")
    .withDescription("The maximum number of pings that can be sent when there is no data/header frame to be sent. gRPC Core will not continue sending pings "
                     "if we run over the limit. Setting it to 0 allows sending pings without such a restriction. If not set, then the default value 2 is used.")
    .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
    .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(PushGrafanaLoki::Properties, std::to_array<core::PropertyReference>({
      KeepAliveTime,
      KeepAliveTimeout,
      MaxPingsWithoutData,
  }));

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  void setUpStreamLabels(core::ProcessContext& context) override;
  nonstd::expected<void, std::string> submitRequest(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) override;
  void setUpGrpcChannel(const std::string& url, core::ProcessContext& context);

  std::string stream_labels_;
  std::chrono::milliseconds connection_timeout_ms_{};
  std::optional<std::string> tenant_id_;
  std::shared_ptr<::grpc::Channel> push_channel_;
  std::unique_ptr<logproto::Pusher::Stub> push_stub_;
};

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
