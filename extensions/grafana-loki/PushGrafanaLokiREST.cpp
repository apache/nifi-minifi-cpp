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
#include "PushGrafanaLokiREST.h"

#include <fstream>
#include <filesystem>

#include "core/Resource.h"
#include "core/ProcessSession.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

void PushGrafanaLokiREST::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PushGrafanaLokiREST::setupClientTimeouts(const core::ProcessContext& context) {
  if (auto connection_timeout = utils::parseOptionalDurationProperty(context, PushGrafanaLokiREST::ConnectTimeout)) {
    client_.setConnectionTimeout(*connection_timeout);
  }

  if (auto read_timeout = utils::parseOptionalDurationProperty(context, PushGrafanaLokiREST::ReadTimeout)) {
    client_.setReadTimeout(*read_timeout);
  }
}

void PushGrafanaLokiREST::setUpStreamLabels(core::ProcessContext& context) {
  stream_label_attributes_ = buildStreamLabelMap(context);
}

void PushGrafanaLokiREST::setAuthorization(const core::ProcessContext& context) {
  if (auto username = context.getProperty(PushGrafanaLokiREST::Username)) {
    auto password = context.getProperty(PushGrafanaLokiREST::Password);
    if (!password) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Username is set, but Password property is not!");
    }
    std::string auth = *username + ":" + *password;
    auto base64_encoded_auth = utils::string::to_base64(auth);
    client_.setRequestHeader("Authorization", "Basic " + base64_encoded_auth);
  } else if (auto bearer_token_file = context.getProperty(PushGrafanaLokiREST::BearerTokenFile)) {
    if (!std::filesystem::exists(*bearer_token_file) || !std::filesystem::is_regular_file(*bearer_token_file)) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bearer Token File is not a regular file!");
    }
    std::ifstream file(*bearer_token_file, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string bearer_token = utils::string::trim(buffer.str());
    client_.setRequestHeader("Authorization", "Bearer " + bearer_token);
  } else {
    client_.setRequestHeader("Authorization", std::nullopt);
  }
}

void PushGrafanaLokiREST::initializeHttpClient(core::ProcessContext& context) {
  auto url = utils::parseProperty(context, Url);
  if (url.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Url property cannot be empty!");
  }
  if (utils::string::endsWith(url, "/")) {
    url += "loki/api/v1/push";
  } else {
    url += "/loki/api/v1/push";
  }
  logger_->log_debug("PushGrafanaLokiREST push url is set to: {}", url);
  client_.initialize(http::HttpRequestMethod::POST, url, getSSLContextService(context));
}

void PushGrafanaLokiREST::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  PushGrafanaLoki::onSchedule(context, session_factory);
  initializeHttpClient(context);
  client_.setContentType("application/json");
  client_.setFollowRedirects(true);
  client_.setRequestHeader("X-Scope-OrgID", context.getProperty(TenantID) | utils::toOptional());

  setupClientTimeouts(context);
  setAuthorization(context);
}

void PushGrafanaLokiREST::addLogLineMetadata(rapidjson::Value& log_line, rapidjson::Document::AllocatorType& allocator, core::FlowFile& flow_file) const {
  if (log_line_metadata_attributes_.empty()) {
    return;
  }

  rapidjson::Value labels(rapidjson::kObjectType);
  for (const auto& label : log_line_metadata_attributes_) {
    auto attribute_value = flow_file.getAttribute(label);
    if (!attribute_value) {
      continue;
    }
    rapidjson::Value label_key(label.c_str(), gsl::narrow<rapidjson::SizeType>(label.length()), allocator);
    rapidjson::Value label_value(attribute_value->c_str(), gsl::narrow<rapidjson::SizeType>(attribute_value->length()), allocator);
    labels.AddMember(label_key, label_value, allocator);
  }
  log_line.PushBack(labels, allocator);
}

std::string PushGrafanaLokiREST::createLokiJson(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) const {
  rapidjson::Document document;
  document.SetObject();
  rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

  rapidjson::Value streams(rapidjson::kArrayType);
  rapidjson::Value stream_object(rapidjson::kObjectType);

  rapidjson::Value labels(rapidjson::kObjectType);
  for (const auto& [key, value] : stream_label_attributes_) {
    rapidjson::Value label(key.c_str(), allocator);
    labels.AddMember(label, rapidjson::Value(value.c_str(), allocator), allocator);
  }

  rapidjson::Value values(rapidjson::kArrayType);
  for (const auto& flow_file : batched_flow_files) {
    rapidjson::Value log_line(rapidjson::kArrayType);

    auto timestamp_str = std::to_string(flow_file->getlineageStartDate().time_since_epoch() / std::chrono::nanoseconds(1));
    rapidjson::Value timestamp;
    timestamp.SetString(timestamp_str.c_str(), gsl::narrow<rapidjson::SizeType>(timestamp_str.length()), allocator);
    rapidjson::Value log_line_value;

    auto line = to_string(session.readBuffer(flow_file));
    log_line_value.SetString(line.c_str(), gsl::narrow<rapidjson::SizeType>(line.length()), allocator);

    log_line.PushBack(timestamp, allocator);
    log_line.PushBack(log_line_value, allocator);
    addLogLineMetadata(log_line, allocator, *flow_file);
    values.PushBack(log_line, allocator);
  }

  stream_object.AddMember("stream", labels, allocator);
  stream_object.AddMember("values", values, allocator);
  streams.PushBack(stream_object, allocator);
  document.AddMember("streams", streams, allocator);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);
  return buffer.GetString();
}

nonstd::expected<void, std::string> PushGrafanaLokiREST::submitRequest(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) {
  auto loki_json = createLokiJson(batched_flow_files, session);
  client_.setPostFields(loki_json);
  if (!client_.submit()) {
    return nonstd::make_unexpected("Submit failed");
  }
  auto response_code = client_.getResponseCode();
  if (response_code < 200 || response_code >= 300) {
    return nonstd::make_unexpected("Error occurred: " + std::to_string(response_code));
  }
  return {};
}

REGISTER_RESOURCE(PushGrafanaLokiREST, Processor);

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
