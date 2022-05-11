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
#include <map>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "../client/HTTPClient.h"
#include "utils/Export.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::processors {

class InvokeHTTP : public core::Processor {
 public:
  SMART_ENUM(InvalidHTTPHeaderFieldHandlingOption,
    (FAIL, "fail"),
    (TRANSFORM, "transform"),
    (DROP, "drop")
  )

  explicit InvokeHTTP(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
    setTriggerWhenEmpty(true);
  }
  EXTENSIONAPI static std::string DefaultContentType;

  EXTENSIONAPI static core::Property Method;
  EXTENSIONAPI static core::Property URL;
  EXTENSIONAPI static core::Property ConnectTimeout;
  EXTENSIONAPI static core::Property ReadTimeout;
  EXTENSIONAPI static core::Property DateHeader;
  EXTENSIONAPI static core::Property FollowRedirects;
  EXTENSIONAPI static core::Property AttributesToSend;
  EXTENSIONAPI static core::Property SSLContext;
  EXTENSIONAPI static core::Property ProxyHost;
  EXTENSIONAPI static core::Property ProxyPort;
  EXTENSIONAPI static core::Property ProxyUsername;
  EXTENSIONAPI static core::Property ProxyPassword;
  EXTENSIONAPI static core::Property ContentType;
  EXTENSIONAPI static core::Property SendBody;
  EXTENSIONAPI static core::Property SendMessageBody;
  EXTENSIONAPI static core::Property UseChunkedEncoding;
  EXTENSIONAPI static core::Property DisablePeerVerification;
  EXTENSIONAPI static core::Property PropPutOutputAttributes;
  EXTENSIONAPI static core::Property AlwaysOutputResponse;
  EXTENSIONAPI static core::Property PenalizeOnNoRetry;
  EXTENSIONAPI static core::Property InvalidHTTPHeaderFieldHandlingStrategy;

  EXTENSIONAPI static const char* STATUS_CODE;
  EXTENSIONAPI static const char* STATUS_MESSAGE;
  EXTENSIONAPI static const char* RESPONSE_BODY;
  EXTENSIONAPI static const char* REQUEST_URL;
  EXTENSIONAPI static const char* TRANSACTION_ID;
  EXTENSIONAPI static const char* REMOTE_DN;
  EXTENSIONAPI static const char* EXCEPTION_CLASS;
  EXTENSIONAPI static const char* EXCEPTION_MESSAGE;

  EXTENSIONAPI static core::Relationship Success;
  EXTENSIONAPI static core::Relationship RelResponse;
  EXTENSIONAPI static core::Relationship RelRetry;
  EXTENSIONAPI static core::Relationship RelNoRetry;
  EXTENSIONAPI static core::Relationship RelFailure;

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 private:
  /**
   * Routes the flowfile to the proper destination
   * @param request request flow file record
   * @param response response flow file record
   * @param session process session
   * @param context process context
   * @param isSuccess success code or not
   * @param statuscode http response code.
   */
  void route(const std::shared_ptr<core::FlowFile> &request, const std::shared_ptr<core::FlowFile> &response, const std::shared_ptr<core::ProcessSession> &session,
             const std::shared_ptr<core::ProcessContext> &context, bool isSuccess, int64_t statusCode);
  bool shouldEmitFlowFile() const;
  std::optional<std::map<std::string, std::string>> validateAttributesAgainstHTTPHeaderRules(const std::map<std::string, std::string>& attributes) const;

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;
  std::string method_;
  std::string url_;
  bool date_header_include_{true};
  std::string attribute_to_send_regex_;
  std::chrono::milliseconds connect_timeout_ms_{20000};
  std::chrono::milliseconds read_timeout_ms_{20000};
  // attribute in which response body will be added
  std::string put_attribute_name_;
  bool always_output_response_{false};
  std::string content_type_;
  bool use_chunked_encoding_{false};
  bool penalize_no_retry_{false};
  // disabling peer verification makes susceptible for MITM attacks
  bool disable_peer_verification_{false};
  utils::HTTPProxy proxy_;
  bool follow_redirects_{true};
  bool send_body_{true};
  InvalidHTTPHeaderFieldHandlingOption invalid_http_header_field_handling_strategy_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<InvokeHTTP>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::processors
