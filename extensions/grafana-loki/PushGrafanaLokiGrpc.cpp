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
#include "PushGrafanaLokiGrpc.h"

#include <utility>

#include "utils/ProcessorConfigUtils.h"
#include "core/Resource.h"
#include "core/ProcessSession.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "google/protobuf/util/time_util.h"
#include "utils/file/FileUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::grafana::loki {

void PushGrafanaLokiGrpc::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PushGrafanaLokiGrpc::setUpStreamLabels(core::ProcessContext& context) {
  auto stream_label_map = buildStreamLabelMap(context);
  std::stringstream formatted_labels;
  bool comma_needed = false;
  formatted_labels << "{";
  for (auto& [label_key, label_value] : stream_label_map) {
    if (comma_needed) {
      formatted_labels << ", ";
    }
    comma_needed = true;

    label_value = utils::string::replaceAll(label_value, "\"", "\\\"");
    formatted_labels << label_key << "=\"" << label_value << "\"";
  }
  formatted_labels << "}";
  stream_labels_ = formatted_labels.str();
}

void PushGrafanaLokiGrpc::setUpGrpcChannel(const std::string& url, core::ProcessContext& context) {
  ::grpc::ChannelArguments args;

  if (auto keep_alive_time = utils::parseOptionalMsProperty(context, PushGrafanaLokiGrpc::KeepAliveTime)) {
    logger_->log_debug("PushGrafanaLokiGrpc Keep Alive Time is set to {} ms", keep_alive_time->count());
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, gsl::narrow<int>(keep_alive_time->count()));
  }

  if (auto keep_alive_timeout = utils::parseOptionalMsProperty(context, PushGrafanaLokiGrpc::KeepAliveTimeout)) {
    logger_->log_debug("PushGrafanaLokiGrpc Keep Alive Timeout is set to {} ms", keep_alive_timeout->count());
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, gsl::narrow<int>(keep_alive_timeout->count()));
  }

  if (auto max_pings_without_data = utils::parseOptionalU64Property(context, PushGrafanaLokiGrpc::MaxPingsWithoutData)) {
    logger_->log_debug("PushGrafanaLokiGrpc Max Pings Without Data is set to {}", *max_pings_without_data);
    args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, gsl::narrow<int>(*max_pings_without_data));
  }

  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

  std::shared_ptr<::grpc::ChannelCredentials> creds;
  if (auto ssl_context_service = getSSLContextService(context)) {
    ::grpc::SslCredentialsOptions ssl_credentials_options;
    ssl_credentials_options.pem_cert_chain = utils::file::FileUtils::get_content(ssl_context_service->getCertificateFile());
    ssl_credentials_options.pem_private_key = utils::file::FileUtils::get_content(ssl_context_service->getPrivateKeyFile());
    ssl_credentials_options.pem_root_certs = utils::file::FileUtils::get_content(ssl_context_service->getCACertificate());
    creds = ::grpc::SslCredentials(ssl_credentials_options);
  } else {
    creds = ::grpc::InsecureChannelCredentials();
  }

  push_channel_ = ::grpc::CreateCustomChannel(url, creds, args);
  if (!push_channel_) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Error creating Loki gRPC channel");
  }

  push_stub_ = logproto::Pusher().NewStub(push_channel_);
}

void PushGrafanaLokiGrpc::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  PushGrafanaLoki::onSchedule(context, session_factory);
  auto url = utils::parseProperty(context, Url);
  if (url.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Url property cannot be empty!");
  }
  tenant_id_ = context.getProperty(TenantID) | utils::toOptional();
  if (auto connection_timeout = utils::parseOptionalMsProperty(context, PushGrafanaLokiGrpc::ConnectTimeout)) {
    connection_timeout_ms_ = *connection_timeout;
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid connection timeout is set.");
  }
  setUpGrpcChannel(url, context);
}

nonstd::expected<void, std::string> PushGrafanaLokiGrpc::submitRequest(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) {
  logproto::PushRequest current_batch;
  logproto::StreamAdapter *stream = current_batch.add_streams();
  stream->set_labels(stream_labels_);

  for (const auto& flow_file : batched_flow_files) {
    logproto::EntryAdapter *entry = stream->add_entries();
    auto timestamp_str = std::to_string(flow_file->getlineageStartDate().time_since_epoch() / std::chrono::nanoseconds(1));
    auto timestamp_nanos = std::stoll(timestamp_str);
    *entry->mutable_timestamp() = google::protobuf::util::TimeUtil::NanosecondsToTimestamp(timestamp_nanos);

    entry->set_line(to_string(session.readBuffer(flow_file)));

    for (const auto& label_attribute : log_line_metadata_attributes_) {
      auto label_value = flow_file->getAttribute(label_attribute);
      if (!label_value) {
        logger_->log_warn("Missing log line attribute in flow_file: {}", label_attribute);
        continue;
      }
      logproto::LabelPairAdapter* label = entry->add_nonindexedlabels();
      label->set_name(label_attribute);
      label->set_value(*label_value);
    }
  }

  if (!push_channel_->WaitForConnected(std::chrono::system_clock::now() + connection_timeout_ms_)) {
    return nonstd::make_unexpected("Timeout waiting for connection to Grafana Loki gRPC server. Please check if the server is running and reachable and the Url value is correct.");
  }

  logproto::PushResponse response;

  ::grpc::ClientContext ctx;
  if (tenant_id_) {
    ctx.AddMetadata("x-scope-orgid", *tenant_id_);
  }
  ::grpc::Status status = push_stub_->Push(&ctx, current_batch, &response);
  if (status.ok()) {
    return {};
  } else {
    return nonstd::make_unexpected(status.error_message());
  }
}

REGISTER_RESOURCE(PushGrafanaLokiGrpc, Processor);

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
