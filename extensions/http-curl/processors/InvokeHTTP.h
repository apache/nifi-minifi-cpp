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

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "../client/HTTPClient.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// InvokeHTTP Class
class InvokeHTTP : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit InvokeHTTP(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
    setTriggerWhenEmpty(true);
  }
  // Destructor
  virtual ~InvokeHTTP();
  // Processor Name
  EXTENSIONAPI static const char *ProcessorName;
  EXTENSIONAPI static std::string DefaultContentType;
  // Supported Properties
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

  EXTENSIONAPI static const char* STATUS_CODE;
  EXTENSIONAPI static const char* STATUS_MESSAGE;
  EXTENSIONAPI static const char* RESPONSE_BODY;
  EXTENSIONAPI static const char* REQUEST_URL;
  EXTENSIONAPI static const char* TRANSACTION_ID;
  EXTENSIONAPI static const char* REMOTE_DN;
  EXTENSIONAPI static const char* EXCEPTION_CLASS;
  EXTENSIONAPI static const char* EXCEPTION_MESSAGE;
  // Supported Relationships
  EXTENSIONAPI static core::Relationship Success;
  EXTENSIONAPI static core::Relationship RelResponse;
  EXTENSIONAPI static core::Relationship RelRetry;
  EXTENSIONAPI static core::Relationship RelNoRetry;
  EXTENSIONAPI static core::Relationship RelFailure;

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  /**
   * Provides a reference to the URL.
   */
  const std::string &getUrl() {
    return url_;
  }

 protected:
  /**
   * Generate a transaction ID
   * @return transaction ID string.
   */
  std::string generateId();

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
  /**
   * Determine if we should emit a new flowfile based on our activity
   * @param method method type
   * @return result of the evaluation.
   */
  bool emitFlowFile(const std::string &method);

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_{nullptr};

  // http method
  std::string method_;
  // url
  std::string url_;
  // include date in the header
  bool date_header_include_{true};
  // attribute to send regex
  std::string attribute_to_send_regex_;
  // connection timeout
  std::chrono::milliseconds connect_timeout_ms_{20000};
  // read timeout.
  std::chrono::milliseconds read_timeout_ms_{20000};
  // attribute in which response body will be added
  std::string put_attribute_name_;
  // determine if we always output a response.
  bool always_output_response_{false};
  // content type.
  std::string content_type_;
  // use chunked encoding.
  bool use_chunked_encoding_{false};
  // penalize on no retry
  bool penalize_no_retry_{false};
  // disable peer verification ( makes susceptible for MITM attacks )
  bool disable_peer_verification_{false};
  utils::HTTPProxy proxy_;
  bool follow_redirects_{true};
  bool send_body_{true};

 private:
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<InvokeHTTP>::getLogger()};
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
