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

#include "PutSplunkHTTP.h"

#include <utility>
#include <vector>

#include "SplunkAttributes.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "http/BaseHTTPClient.h"
#include "http/HTTPClient.h"
#include "rapidjson/document.h"
#include "utils/ByteArrayCallback.h"
#include "utils/OptionalUtils.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::extensions::splunk {

void PutSplunkHTTP::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

std::string PutSplunkHTTP::getEndpoint(http::HTTPClient& client) {
  std::stringstream endpoint;
  endpoint << "/services/collector/raw";
  std::vector<std::string> parameters;
  if (source_type_) {
    parameters.push_back("sourcetype=" + client.escape(*source_type_));
  }
  if (source_) {
    parameters.push_back("source=" + client.escape(*source_));
  }
  if (host_) {
    parameters.push_back("host=" + client.escape(*host_));
  }
  if (index_) {
    parameters.push_back("index=" + client.escape(*index_));
  }
  if (!parameters.empty()) {
    endpoint << "?" << utils::string::join("&", parameters);
  }
  return endpoint.str();
}

namespace {
std::optional<std::string> getContentType(core::ProcessContext& context, const core::FlowFile& flow_file) {
  return context.getProperty(PutSplunkHTTP::ContentType) | utils::toOptional() | utils::orElse ([&flow_file] {return flow_file.getAttribute("mime.type");});
}

bool setAttributesFromClientResponse(core::FlowFile& flow_file, http::HTTPClient& client, const std::shared_ptr<core::logging::Logger>& logger) {
  rapidjson::Document response_json;
  rapidjson::ParseResult parse_result = response_json.Parse<rapidjson::kParseStopWhenDoneFlag>(client.getResponseBody().data());
  bool result = true;
  if (parse_result.IsError()) {
    logger->log_error("Failed to parse Splunk HEC response JSON");
    return false;
  }

  if (response_json.HasMember("code") && response_json["code"].IsInt()) {
    auto code = response_json["code"].GetInt();
    flow_file.setAttribute(SPLUNK_RESPONSE_CODE, std::to_string(code));
    if (code != 0) {
      logger->log_error("Splunk HEC returned error code: {}", code);
      result = false;
    }
  } else {
    logger->log_error("Splunk HEC response JSON does not contain a valid 'code' field");
    result = false;
  }

  if (response_json.HasMember("ackId") && response_json["ackId"].IsUint64()) {
    flow_file.setAttribute(SPLUNK_ACK_ID, std::to_string(response_json["ackId"].GetUint64()));
  } else {
    logger->log_error("Splunk HEC response JSON does not contain a valid 'ackId' field");
    result = false;
  }

  return result;
}

bool enrichFlowFileWithAttributes(core::FlowFile& flow_file, http::HTTPClient& client, const std::shared_ptr<core::logging::Logger>& logger) {
  flow_file.setAttribute(SPLUNK_STATUS_CODE, std::to_string(client.getResponseCode()));
  flow_file.setAttribute(SPLUNK_RESPONSE_TIME, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));

  auto result = true;
  if (client.getResponseCode() != 200) {
    logger->log_error("Received failure response code from Splunk HEC: {}", client.getResponseCode());
    result = false;
  }

  if (!setAttributesFromClientResponse(flow_file, client, logger)) {
    return false;
  }

  return result;
}

void setFlowFileAsPayload(core::ProcessSession& session,
                          core::ProcessContext& context,
                          http::HTTPClient& client,
                          const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file) {
  auto payload = std::make_unique<http::HTTPUploadByteArrayInputCallback>();
  session.read(flow_file, std::ref(*payload));
  payload->pos = 0;
  client.setRequestHeader("Content-Length", std::to_string(flow_file->getSize()));
  client.setPostSize(flow_file->getSize());

  client.setUploadCallback(std::move(payload));

  if (auto content_type = getContentType(context, *flow_file)) {
    client.setContentType(content_type.value());
  }
}
}  // namespace

void PutSplunkHTTP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  SplunkHECProcessor::onSchedule(context, session_factory);
  ssl_context_service_ = utils::parseOptionalControllerService<minifi::controllers::SSLContextServiceInterface>(context, SSLContext, getUUID());
  auto create_client = [this]() -> std::unique_ptr<minifi::http::HTTPClient> {
    auto client = std::make_unique<http::HTTPClient>();
    initializeClient(*client, getNetworkLocation().append(getEndpoint(*client)), ssl_context_service_);
    return client;
  };

  client_queue_ = utils::ResourceQueue<http::HTTPClient>::create(create_client, context.getMaxConcurrentTasks(), std::nullopt, logger_);
  source_type_ = utils::parseOptionalProperty(context, PutSplunkHTTP::SourceType);
  source_ = utils::parseOptionalProperty(context, PutSplunkHTTP::Source);
  host_ = utils::parseOptionalProperty(context, PutSplunkHTTP::Host);
  index_ = utils::parseOptionalProperty(context, PutSplunkHTTP::Index);
}

void PutSplunkHTTP::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(client_queue_);

  auto ff = session.get();
  if (!ff) {
    context.yield();
    return;
  }
  auto flow_file = gsl::not_null(std::move(ff));

  auto client = client_queue_->getResource();

  setFlowFileAsPayload(session, context, *client, flow_file);

  bool success = false;
  if (client->submit()) {
    success = enrichFlowFileWithAttributes(*flow_file, *client, logger_);
  } else {
    logger_->log_error("Failed to submit HTTP request to Splunk HEC");
  }

  session.transfer(flow_file, success ? Success : Failure);
}

REGISTER_RESOURCE(PutSplunkHTTP, Processor);

}  // namespace org::apache::nifi::minifi::extensions::splunk
