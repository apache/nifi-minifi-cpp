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

#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "client/HTTPClient.h"
#include "utils/HTTPClient.h"
#include "utils/TimeUtil.h"

#include "rapidjson/document.h"


namespace org::apache::nifi::minifi::extensions::splunk {

const core::Property PutSplunkHTTP::Source(core::PropertyBuilder::createProperty("Source")
    ->withDescription("Basic field describing the source of the event. If unspecified, the event will use the default defined in splunk.")
    ->supportsExpressionLanguage(true)->build());

const core::Property PutSplunkHTTP::SourceType(core::PropertyBuilder::createProperty("Source Type")
    ->withDescription("Basic field describing the source type of the event. If unspecified, the event will use the default defined in splunk.")
    ->supportsExpressionLanguage(true)->build());

const core::Property PutSplunkHTTP::Host(core::PropertyBuilder::createProperty("Host")
    ->withDescription("Basic field describing the host of the event. If unspecified, the event will use the default defined in splunk.")
    ->supportsExpressionLanguage(true)->build());

const core::Property PutSplunkHTTP::Index(core::PropertyBuilder::createProperty("Index")
    ->withDescription("Identifies the index where to send the event. If unspecified, the event will use the default defined in splunk.")
    ->supportsExpressionLanguage(true)->build());

const core::Property PutSplunkHTTP::ContentType(core::PropertyBuilder::createProperty("Content Type")
    ->withDescription("The media type of the event sent to Splunk. If not set, \"mime.type\" flow file attribute will be used. "
                      "In case of neither of them is specified, this information will not be sent to the server.")
    ->supportsExpressionLanguage(true)->build());


const core::Relationship PutSplunkHTTP::Success("success", "FlowFiles that are sent successfully to the destination are sent to this relationship.");
const core::Relationship PutSplunkHTTP::Failure("failure", "FlowFiles that failed to send to the destination are sent to this relationship.");

void PutSplunkHTTP::initialize() {
  SplunkHECProcessor::initialize();
  setSupportedRelationships({Success, Failure});
  updateSupportedProperties({Source, SourceType, Host, Index, ContentType});
}

void PutSplunkHTTP::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) {
  SplunkHECProcessor::onSchedule(context, sessionFactory);
}


namespace {
std::optional<std::string> getContentType(core::ProcessContext& context, const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file) {
  std::optional<std::string> content_type = context.getProperty(PutSplunkHTTP::ContentType);
  if (content_type.has_value())
    return content_type;
  return flow_file->getAttribute("mime.key");
}


std::string getEndpoint(core::ProcessContext& context, const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file) {
  std::stringstream endpoint;
  endpoint << "/services/collector/raw";
  std::vector<std::string> parameters;
  std::string prop_value;
  if (context.getProperty(PutSplunkHTTP::SourceType, prop_value, flow_file)) {
    parameters.push_back("sourcetype=" + prop_value);
  }
  if (context.getProperty(PutSplunkHTTP::Source, prop_value, flow_file)) {
    parameters.push_back("source=" + prop_value);
  }
  if (context.getProperty(PutSplunkHTTP::Host, prop_value, flow_file)) {
    parameters.push_back("host=" + prop_value);
  }
  if (context.getProperty(PutSplunkHTTP::Index, prop_value, flow_file)) {
    parameters.push_back("index=" + prop_value);
  }
  if (!parameters.empty()) {
    endpoint << "?" << utils::StringUtils::join("&", parameters);
  }
  return endpoint.str();
}

bool addAttributesFromClientResponse(core::FlowFile& flow_file, utils::HTTPClient& client) {
  rapidjson::Document response_json;
  rapidjson::ParseResult parse_result = response_json.Parse<rapidjson::kParseStopWhenDoneFlag>(client.getResponseBody().data());
  bool result = true;
  if (parse_result.IsError())
    return false;

  if (response_json.HasMember("code") && response_json["code"].IsInt())
    flow_file.addAttribute(SPLUNK_RESPONSE_CODE, std::to_string(response_json["code"].GetInt()));
  else
    result = false;

  if (response_json.HasMember("ackId") && response_json["ackId"].IsUint64())
    flow_file.addAttribute(SPLUNK_ACK_ID, std::to_string(response_json["ackId"].GetUint64()));
  else
    result = false;

  return result;
}

bool enrichFlowFileWithAttributes(core::FlowFile& flow_file, utils::HTTPClient& client) {
  flow_file.addAttribute(SPLUNK_STATUS_CODE, std::to_string(client.getResponseCode()));
  flow_file.addAttribute(SPLUNK_RESPONSE_TIME, std::to_string(utils::timeutils::getTimeStamp<std::chrono::milliseconds>(std::chrono::system_clock::now())));

  return addAttributesFromClientResponse(flow_file, client) && client.getResponseCode() == 200;
}

void setFlowFileAsPayload(core::ProcessSession& session,
                                         core::ProcessContext& context,
                                         utils::HTTPClient& client,
                                         const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file,
                                         const std::unique_ptr<utils::ByteInputCallBack>& payload_callback,
                                         const std::unique_ptr<utils::HTTPUploadCallback>& payload_callback_obj) {
  session.read(flow_file, payload_callback.get());
  payload_callback_obj->ptr = payload_callback.get();
  payload_callback_obj->pos = 0;
  client.appendHeader("Content-Length", std::to_string(flow_file->getSize()));

  client.setUploadCallback(payload_callback_obj.get());
  client.setSeekFunction(payload_callback_obj.get());

  auto content_type = getContentType(context, flow_file);
  if (content_type.has_value())
    client.setContentType(content_type.value());
}
}  // namespace

void PutSplunkHTTP::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session);

  auto ff = session->get();
  if (!ff) {
    context->yield();
    return;
  }
  auto flow_file = gsl::not_null(std::move(ff));

  utils::HTTPClient client(getUrl() + getEndpoint(*context, flow_file), getSSLContextService(*context));
  setHeaders(client);

  std::unique_ptr<utils::ByteInputCallBack> payload_callback = std::make_unique<utils::ByteInputCallBack>();
  std::unique_ptr<utils::HTTPUploadCallback> payload_callback_obj = std::make_unique<utils::HTTPUploadCallback>();
  setFlowFileAsPayload(*session, *context, client, flow_file, payload_callback, payload_callback_obj);

  bool success = false;
  if (client.submit())
    success = enrichFlowFileWithAttributes(*flow_file, client);

  session->transfer(flow_file, success ? Success : Failure);
}


REGISTER_RESOURCE(PutSplunkHTTP, "Sends the flow file contents to the specified Splunk HTTP Event Collector over HTTP or HTTPS. Supports HEC Index Acknowledgement.");

}  // namespace org::apache::nifi::minifi::extensions::splunk

