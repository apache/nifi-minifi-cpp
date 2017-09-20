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
#ifndef __INVOKE_HTTP_H__
#define __INVOKE_HTTP_H__

#include <memory>
#include <regex>

#include <curl/curl.h>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/Resource.h"
#include "controllers/SSLContextService.h"
#include "utils/ByteInputCallBack.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "utils/HTTPClient.h"

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
  InvokeHTTP(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid),
        date_header_include_(true),
        connect_timeout_(20000),
        penalize_no_retry_(false),
        read_timeout_(20000),
        always_output_response_(false),
        disable_peer_verification_(false),
        ssl_context_service_(nullptr),
        use_chunked_encoding_(false),
        logger_(logging::LoggerFactory<InvokeHTTP>::getLogger()) {
    static utils::HTTPClientInitializer *initializer = utils::HTTPClientInitializer::getInstance();
  }
  // Destructor
  virtual ~InvokeHTTP();
  // Processor Name
  static const char *ProcessorName;
  // Supported Properties
  static core::Property Method;
  static core::Property URL;
  static core::Property ConnectTimeout;
  static core::Property ReadTimeout;
  static core::Property DateHeader;
  static core::Property FollowRedirects;
  static core::Property AttributesToSend;
  static core::Property SSLContext;
  static core::Property ProxyHost;
  static core::Property ProxyPort;
  static core::Property ProxyUser;
  static core::Property ProxyPassword;
  static core::Property ContentType;
  static core::Property SendBody;
  static core::Property UseChunkedEncoding;
  static core::Property DisablePeerVerification;
  static core::Property PropPutOutputAttributes;

  static core::Property AlwaysOutputResponse;

  static core::Property PenalizeOnNoRetry;

  static const char* STATUS_CODE;
  static const char* STATUS_MESSAGE;
  static const char* RESPONSE_BODY;
  static const char* REQUEST_URL;
  static const char* TRANSACTION_ID;
  static const char* REMOTE_DN;
  static const char* EXCEPTION_CLASS;
  static const char* EXCEPTION_MESSAGE;
  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship RelResponse;
  static core::Relationship RelRetry;
  static core::Relationship RelNoRetry;
  static core::Relationship RelFailure;

  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  virtual void initialize();
  virtual void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);
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
  void route(std::shared_ptr<FlowFileRecord> &request, std::shared_ptr<FlowFileRecord> &response, core::ProcessSession *session, core::ProcessContext *context, bool isSuccess, int statusCode);
  /**
   * Determine if we should emit a new flowfile based on our activity
   * @param method method type
   * @return result of the evaluation.
   */
  bool emitFlowFile(const std::string &method);

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

  // http method
  std::string method_;
  // url
  std::string url_;
  // include date in the header
  bool date_header_include_;
  // attribute to send regex
  std::string attribute_to_send_regex_;
  // connection timeout
  int64_t connect_timeout_;
  // read timeout.
  int64_t read_timeout_;
  // attribute in which response body will be added
  std::string put_attribute_name_;
  // determine if we always output a response.
  bool always_output_response_;
  // penalize on no retry
  bool penalize_no_retry_;
  // content type.
  std::string content_type_;
  // use chunked encoding.
  bool use_chunked_encoding_;
  // disable peer verification ( makes susceptible for MITM attacks )
  bool disable_peer_verification_;
 private:
  std::shared_ptr<logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

REGISTER_RESOURCE(InvokeHTTP)

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
