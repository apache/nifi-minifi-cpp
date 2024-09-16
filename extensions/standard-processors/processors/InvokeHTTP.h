/**
 * InvokeHTTP class declaration
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

#include <curl/curl.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/Core.h"
#include "core/OutputAttributeDefinition.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Id.h"
#include "utils/ResourceQueue.h"
#include "../http/HTTPClient.h"
#include "utils/Export.h"
#include "utils/Enum.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

namespace invoke_http {
enum class InvalidHTTPHeaderFieldHandlingOption {
    fail,
    transform,
    drop
};
}  // namespace invoke_http

class InvokeHTTP : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr std::string_view STATUS_CODE = "invokehttp.status.code";
  EXTENSIONAPI static constexpr std::string_view STATUS_MESSAGE = "invokehttp.status.message";
  EXTENSIONAPI static constexpr std::string_view REQUEST_URL = "invokehttp.request.url";
  EXTENSIONAPI static constexpr std::string_view TRANSACTION_ID = "invokehttp.tx.id";

  explicit InvokeHTTP(std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
    setTriggerWhenEmpty(true);
  }

  EXTENSIONAPI static constexpr const char* Description = "An HTTP client processor which can interact with a configurable HTTP Endpoint. "
                                                          "The destination URL and HTTP Method are configurable. FlowFile attributes are converted to HTTP headers and the "
                                                          "FlowFile contents are included as the body of the request (if the HTTP Method is PUT, POST or PATCH).";

  EXTENSIONAPI static constexpr auto Method = core::PropertyDefinitionBuilder<magic_enum::enum_count<http::HttpRequestMethod>()>::createProperty("HTTP Method")
      .withDescription("HTTP request method. Methods other than POST, PUT and PATCH will be sent without a message body.")
      .withAllowedValues(magic_enum::enum_names<http::HttpRequestMethod>())
      .withDefaultValue(magic_enum::enum_name(http::HttpRequestMethod::GET))
      .build();
  EXTENSIONAPI static constexpr auto URL = core::PropertyDefinitionBuilder<>::createProperty("Remote URL")
      .withDescription("Remote URL which will be connected to, including scheme, host, port, path.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ConnectTimeout = core::PropertyDefinitionBuilder<>::createProperty("Connection Timeout")
      .withDescription("Max wait time for connection to remote service")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("5 s")
      .build();
  EXTENSIONAPI static constexpr auto ReadTimeout = core::PropertyDefinitionBuilder<>::createProperty("Read Timeout")
      .withDescription("Max wait time for response from remote service")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("15 s")
      .build();
  EXTENSIONAPI static constexpr auto DateHeader = core::PropertyDefinitionBuilder<>::createProperty("Include Date Header")
      .withDescription("Include an RFC-2616 Date header in the request.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto FollowRedirects = core::PropertyDefinitionBuilder<>::createProperty("Follow Redirects")
      .withDescription("Follow HTTP redirects issued by remote server.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto AttributesToSend = core::PropertyDefinitionBuilder<>::createProperty("Attributes to Send")
      .withDescription("Regular expression that defines which attributes to send as HTTP headers in the request. If not defined, no attributes are sent as headers.")
      .build();
  EXTENSIONAPI static constexpr auto SSLContext = core::PropertyDefinitionBuilder<0, 0, 1>::createProperty("SSL Context Service")
      .withDescription("The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.")
      .isRequired(false)
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .withExclusiveOfProperties({{{"Remote URL", "^http:.*$"}}})
      .build();
  EXTENSIONAPI static constexpr auto ProxyHost = core::PropertyDefinitionBuilder<>::createProperty("Proxy Host")
      .withDescription("The fully qualified hostname or IP address of the proxy server")
      .build();
  EXTENSIONAPI static constexpr auto ProxyPort = core::PropertyDefinitionBuilder<>::createProperty("Proxy Port")
      .withDescription("The port of the proxy server")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto ProxyUsername = core::PropertyDefinitionBuilder<>::createProperty("invokehttp-proxy-username", "Proxy Username")
      .withDescription("Username to set when authenticating against proxy")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto ProxyPassword = core::PropertyDefinitionBuilder<>::createProperty("invokehttp-proxy-password", "Proxy Password")
      .withDescription("Password to set when authenticating against proxy")
      .isRequired(false)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto ContentType = core::PropertyDefinitionBuilder<>::createProperty("Content-type")
      .withDescription("The Content-Type to specify for when content is being transmitted through a PUT, "
          "POST or PATCH. In the case of an empty value after evaluating an expression language expression, "
          "Content-Type defaults to")
      .withDefaultValue("application/octet-stream")
      .build();
  EXTENSIONAPI static constexpr auto SendBody = core::PropertyDefinitionBuilder<>::createProperty("send-message-body", "Send Body")
      .withDescription("DEPRECATED. Only kept for backwards compatibility, no functionality is included.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto SendMessageBody = core::PropertyDefinitionBuilder<>::createProperty("Send Message Body")
      .withDescription("If true, sends the HTTP message body on POST/PUT/PATCH requests (default). "
          "If false, suppresses the message body and content-type header for these requests.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto UseChunkedEncoding = core::PropertyDefinitionBuilder<>::createProperty("Use Chunked Encoding")
      .withDescription("When POST'ing, PUT'ing or PATCH'ing content set this property to true in order to not pass the 'Content-length' header"
          " and instead send 'Transfer-Encoding' with a value of 'chunked'."
          " This will enable the data transfer mechanism which was introduced in HTTP 1.1 to pass data of unknown lengths in chunks.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto DisablePeerVerification = core::PropertyDefinitionBuilder<>::createProperty("Disable Peer Verification")
      .withDescription("DEPRECATED. The value is ignored, peer and host verification are always performed when using SSL/TLS.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto PutResponseBodyInAttribute = core::PropertyDefinitionBuilder<>::createProperty("Put Response Body in Attribute")
      .withDescription("If set, the response body received back will be put into an attribute of the original "
          "FlowFile instead of a separate FlowFile. "
          "The attribute key to put to is determined by evaluating value of this property. ")
      .build();
  EXTENSIONAPI static constexpr auto AlwaysOutputResponse = core::PropertyDefinitionBuilder<>::createProperty("Always Output Response")
      .withDescription("Will force a response FlowFile to be generated and routed to the 'Response' relationship regardless of what the server status code received is ")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto PenalizeOnNoRetry = core::PropertyDefinitionBuilder<>::createProperty("Penalize on \"No Retry\"")
      .withDescription("Enabling this property will penalize FlowFiles that are routed to the \"No Retry\" relationship.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto InvalidHTTPHeaderFieldHandlingStrategy
    = core::PropertyDefinitionBuilder<magic_enum::enum_count<invoke_http::InvalidHTTPHeaderFieldHandlingOption>()>::createProperty(
          "Invalid HTTP Header Field Handling Strategy")
      .withDescription("Indicates what should happen when an attribute's name is not a valid HTTP header field name. "
          "Options: transform - invalid characters are replaced, fail - flow file is transferred to failure, drop - drops invalid attributes from HTTP message")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(invoke_http::InvalidHTTPHeaderFieldHandlingOption::transform))
      .withAllowedValues(magic_enum::enum_names<invoke_http::InvalidHTTPHeaderFieldHandlingOption>())
      .build();
  EXTENSIONAPI static constexpr auto UploadSpeedLimit = core::PropertyDefinitionBuilder<>::createProperty("Upload Speed Limit")
      .withDescription("Maximum upload speed, e.g. '500 KB/s'. Leave this empty if you want no limit.")
      .withPropertyType(core::StandardPropertyTypes::DATA_TRANSFER_SPEED_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto DownloadSpeedLimit = core::PropertyDefinitionBuilder<>::createProperty("Download Speed Limit")
      .withDescription("Maximum download speed,e.g. '500 KB/s'. Leave this empty if you want no limit.")
      .withPropertyType(core::StandardPropertyTypes::DATA_TRANSFER_SPEED_TYPE)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
        Method,
        URL,
        ConnectTimeout,
        ReadTimeout,
        DateHeader,
        FollowRedirects,
        AttributesToSend,
        SSLContext,
        ProxyHost,
        ProxyPort,
        ProxyUsername,
        ProxyPassword,
        ContentType,
        SendBody,
        SendMessageBody,
        UseChunkedEncoding,
        DisablePeerVerification,
        PutResponseBodyInAttribute,
        AlwaysOutputResponse,
        PenalizeOnNoRetry,
        InvalidHTTPHeaderFieldHandlingStrategy,
        UploadSpeedLimit,
        DownloadSpeedLimit
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "The original FlowFile will be routed upon success (2xx status codes). It will have new attributes detailing the success of the request."};
  EXTENSIONAPI static constexpr auto RelResponse = core::RelationshipDefinition{"response",
      "A Response FlowFile will be routed upon success (2xx status codes). "
      "If the 'Always Output Response' property is true then the response will be sent "
      "to this relationship regardless of the status code received."};
  EXTENSIONAPI static constexpr auto RelRetry = core::RelationshipDefinition{"retry",
      "The original FlowFile will be routed on any status code that can be retried "
      "(5xx status codes). It will have new attributes detailing the request."};
  EXTENSIONAPI static constexpr auto RelNoRetry = core::RelationshipDefinition{"no retry",
      "The original FlowFile will be routed on any status code that should NOT "
      "be retried (1xx, 3xx, 4xx status codes). It will have new attributes detailing the request."};
  EXTENSIONAPI static constexpr auto RelFailure = core::RelationshipDefinition{"failure",
      "The original FlowFile will be routed on any type of connection failure, "
      "timeout or general exception. It will have new attributes detailing the request."};

  EXTENSIONAPI static constexpr auto Relationships = std::array{
        Success,
        RelResponse,
        RelRetry,
        RelNoRetry,
        RelFailure
  };

  EXTENSIONAPI static constexpr auto StatusCode = core::OutputAttributeDefinition<4>{STATUS_CODE, { Success, RelResponse, RelRetry, RelNoRetry },
      "The status code that is returned"};
  EXTENSIONAPI static constexpr auto StatusMessage = core::OutputAttributeDefinition<4>{STATUS_MESSAGE, { Success, RelResponse, RelRetry, RelNoRetry },
      "The status message that is returned"};
  EXTENSIONAPI static constexpr auto RequestUrl = core::OutputAttributeDefinition<4>{REQUEST_URL, { Success, RelResponse, RelRetry, RelNoRetry },
      "The original request URL"};
  EXTENSIONAPI static constexpr auto TxId = core::OutputAttributeDefinition<4>{TRANSACTION_ID, { Success, RelResponse, RelRetry, RelNoRetry },
      "The transaction ID that is returned after reading the response"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 4>{
      StatusCode,
      StatusMessage,
      RequestUrl,
      TxId
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static std::string DefaultContentType;

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 private:
  void route(const std::shared_ptr<core::FlowFile>& request, const std::shared_ptr<core::FlowFile>& response, core::ProcessSession& session,
             core::ProcessContext& context, bool is_success, int64_t status_code);
  [[nodiscard]] bool shouldEmitFlowFile() const;
  void onTriggerWithClient(core::ProcessContext& context, core::ProcessSession& session,
                           const std::shared_ptr<core::FlowFile>& flow_file, minifi::http::HTTPClient& client);
  [[nodiscard]] bool appendHeaders(const core::FlowFile& flow_file, /*std::invocable<std::string, std::string>*/ auto append_header);


  void setupMembersFromProperties(const core::ProcessContext& context);
  std::unique_ptr<minifi::http::HTTPClient> createHTTPClientFromMembers() const;

  http::HttpRequestMethod method_{};
  std::optional<utils::Regex> attributes_to_send_;
  std::optional<std::string> put_response_body_in_attribute_;
  bool always_output_response_{false};
  bool use_chunked_encoding_{false};
  bool penalize_no_retry_{false};
  bool send_message_body_{true};
  bool send_date_header_{true};
  core::DataTransferSpeedValue maximum_upload_speed_{0};
  core::DataTransferSpeedValue maximum_download_speed_{0};

  std::string url_;
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

  std::chrono::milliseconds connect_timeout_{std::chrono::seconds(30)};
  std::chrono::milliseconds read_timeout_{std::chrono::seconds(30)};

  http::HTTPProxy proxy_{};
  bool follow_redirects_ = false;
  std::optional<std::string> content_type_;


  invoke_http::InvalidHTTPHeaderFieldHandlingOption invalid_http_header_field_handling_strategy_{};

  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<InvokeHTTP>::getLogger(uuid_)};
  std::shared_ptr<utils::ResourceQueue<http::HTTPClient>> client_queue_;
};

}  // namespace org::apache::nifi::minifi::processors
