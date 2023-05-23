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

#include <vector>
#include <utility>

#include "SplunkAttributes.h"

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "client/HTTPClient.h"
#include "utils/BaseHTTPClient.h"
#include "utils/OptionalUtils.h"
#include "utils/ByteArrayCallback.h"

#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::extensions::splunk {

void PutSplunkHTTP::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

namespace {
std::optional<std::string> getContentType(core::ProcessContext& context, const core::FlowFile& flow_file) {
  return context.getProperty(PutSplunkHTTP::ContentType) | utils::orElse ([&flow_file] {return flow_file.getAttribute("mime.type");});
}

std::string getEndpoint(core::ProcessContext& context, curl::HTTPClient& client) {
  std::stringstream endpoint;
  endpoint << "/services/collector/raw";
  std::vector<std::string> parameters;
  if (auto source_type = context.getProperty(PutSplunkHTTP::SourceType)) {
    parameters.push_back("sourcetype=" + client.escape(*source_type));
  }
  if (auto source = context.getProperty(PutSplunkHTTP::Source)) {
    parameters.push_back("source=" + client.escape(*source));
  }
  if (auto host = context.getProperty(PutSplunkHTTP::Host)) {
    parameters.push_back("host=" + client.escape(*host));
  }
  if (auto index = context.getProperty(PutSplunkHTTP::Index)) {
    parameters.push_back("index=" + client.escape(*index));
  }
  if (!parameters.empty()) {
    endpoint << "?" << utils::StringUtils::join("&", parameters);
  }
  return endpoint.str();
}

bool setAttributesFromClientResponse(core::FlowFile& flow_file, curl::HTTPClient& client) {
  rapidjson::Document response_json;
  rapidjson::ParseResult parse_result = response_json.Parse<rapidjson::kParseStopWhenDoneFlag>(client.getResponseBody().data());
  bool result = true;
  if (parse_result.IsError())
    return false;

  if (response_json.HasMember("code") && response_json["code"].IsInt())
    flow_file.setAttribute(SPLUNK_RESPONSE_CODE, std::to_string(response_json["code"].GetInt()));
  else
    result = false;

  if (response_json.HasMember("ackId") && response_json["ackId"].IsUint64())
    flow_file.setAttribute(SPLUNK_ACK_ID, std::to_string(response_json["ackId"].GetUint64()));
  else
    result = false;

  return result;
}

bool enrichFlowFileWithAttributes(core::FlowFile& flow_file, curl::HTTPClient& client) {
  flow_file.setAttribute(SPLUNK_STATUS_CODE, std::to_string(client.getResponseCode()));
  flow_file.setAttribute(SPLUNK_RESPONSE_TIME, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));

  return setAttributesFromClientResponse(flow_file, client) && client.getResponseCode() == 200;
}

void setFlowFileAsPayload(core::ProcessSession& session,
                          core::ProcessContext& context,
                          curl::HTTPClient& client,
                          const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file) {
  auto payload = std::make_unique<utils::HTTPUploadByteArrayInputCallback>();
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

void PutSplunkHTTP::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) {
  SplunkHECProcessor::onSchedule(context, sessionFactory);
  std::weak_ptr<core::ProcessContext> weak_context = context;
  auto create_client = [this, weak_context]() -> std::unique_ptr<minifi::extensions::curl::HTTPClient> {
    if (auto context = weak_context.lock()) {
      auto client = std::make_unique<curl::HTTPClient>();
      initializeClient(*client, getNetworkLocation().append(getEndpoint(*context, *client)), getSSLContextService(*context));
      return client;
    }
    return nullptr;
  };

  client_queue_ = utils::ResourceQueue<extensions::curl::HTTPClient>::create(create_client, getMaxConcurrentTasks(), std::nullopt, logger_);
}

void PutSplunkHTTP::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session && client_queue_);

  auto ff = session->get();
  if (!ff) {
    context->yield();
    return;
  }
  auto flow_file = gsl::not_null(std::move(ff));

  auto client = client_queue_->getResource();

  setFlowFileAsPayload(*session, *context, *client, flow_file);

  bool success = false;
  if (client->submit())
    success = enrichFlowFileWithAttributes(*flow_file, *client);

  session->transfer(flow_file, success ? Success : Failure);
}

}  // namespace org::apache::nifi::minifi::extensions::splunk
