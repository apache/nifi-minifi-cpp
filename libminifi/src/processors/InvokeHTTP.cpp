/**
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

#include "processors/InvokeHTTP.h"
#include <regex.h>
#include <curl/curlbuild.h>
#include <curl/easy.h>
#include <uuid/uuid.h>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "io/DataStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const char *InvokeHTTP::ProcessorName = "InvokeHTTP";

core::Property InvokeHTTP::Method("HTTP Method", "HTTP request method (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS). "
                                  "Arbitrary methods are also supported. Methods other than POST, PUT and PATCH will be sent without a message body.",
                                  "GET");
core::Property InvokeHTTP::URL("Remote URL", "Remote URL which will be connected to, including scheme, host, port, path.", "");
core::Property InvokeHTTP::ConnectTimeout("Connection Timeout", "Max wait time for connection to remote service.", "5 secs");
core::Property InvokeHTTP::ReadTimeout("Read Timeout", "Max wait time for response from remote service.", "15 secs");
core::Property InvokeHTTP::DateHeader("Include Date Header", "Include an RFC-2616 Date header in the request.", "True");
core::Property InvokeHTTP::FollowRedirects("Follow Redirects", "Follow HTTP redirects issued by remote server.", "True");
core::Property InvokeHTTP::AttributesToSend("Attributes to Send", "Regular expression that defines which attributes to send as HTTP"
                                            " headers in the request. If not defined, no attributes are sent as headers.",
                                            "");
core::Property InvokeHTTP::SSLContext("SSL Context Service", "The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.", "");
core::Property InvokeHTTP::ProxyHost("Proxy Host", "The fully qualified hostname or IP address of the proxy server", "");
core::Property InvokeHTTP::ProxyPort("Proxy Port", "The port of the proxy server", "");
core::Property InvokeHTTP::ProxyUser("invokehttp-proxy-user", "Username to set when authenticating against proxy", "");
core::Property InvokeHTTP::ProxyPassword("invokehttp-proxy-password", "Password to set when authenticating against proxy", "");
core::Property InvokeHTTP::ContentType("Content-type", "The Content-Type to specify for when content is being transmitted through a PUT, "
                                       "POST or PATCH. In the case of an empty value after evaluating an expression language expression, "
                                       "Content-Type defaults to",
                                       "application/octet-stream");
core::Property InvokeHTTP::SendBody("send-message-body", "If true, sends the HTTP message body on POST/PUT/PATCH requests (default).  "
                                    "If false, suppresses the message body and content-type header for these requests.",
                                    "true");

core::Property InvokeHTTP::PropPutOutputAttributes("Put Response Body in Attribute", "If set, the response body received back will be put into an attribute of the original "
                                                   "FlowFile instead of a separate FlowFile. The attribute key to put to is determined by evaluating value of this property. ",
                                                   "");
core::Property InvokeHTTP::AlwaysOutputResponse("Always Output Response", "Will force a response FlowFile to be generated and routed to the 'Response' relationship "
                                                "regardless of what the server status code received is ",
                                                "false");
core::Property InvokeHTTP::PenalizeOnNoRetry("Penalize on \"No Retry\"", "Enabling this property will penalize FlowFiles that are routed to the \"No Retry\" relationship.", "false");

const char* InvokeHTTP::STATUS_CODE = "invokehttp.status.code";
const char* InvokeHTTP::STATUS_MESSAGE = "invokehttp.status.message";
const char* InvokeHTTP::RESPONSE_BODY = "invokehttp.response.body";
const char* InvokeHTTP::REQUEST_URL = "invokehttp.request.url";
const char* InvokeHTTP::TRANSACTION_ID = "invokehttp.tx.id";
const char* InvokeHTTP::REMOTE_DN = "invokehttp.remote.dn";
const char* InvokeHTTP::EXCEPTION_CLASS = "invokehttp.java.exception.class";
const char* InvokeHTTP::EXCEPTION_MESSAGE = "invokehttp.java.exception.message";

core::Relationship InvokeHTTP::Success("success", "All files are routed to success");

core::Relationship InvokeHTTP::RelResponse("response", "Represents a response flowfile");

core::Relationship InvokeHTTP::RelRetry("retry", "The original FlowFile will be routed on any status code that can be retried "
                                        "(5xx status codes). It will have new attributes detailing the request.");

core::Relationship InvokeHTTP::RelNoRetry("no retry", "The original FlowFile will be routed on any status code that should NOT "
                                          "be retried (1xx, 3xx, 4xx status codes). It will have new attributes detailing the request.");

core::Relationship InvokeHTTP::RelFailure("failure", "The original FlowFile will be routed on any type of connection failure, "
                                          "timeout or general exception. It will have new attributes detailing the request.");

void InvokeHTTP::set_request_method(CURL *curl, const std::string &method) {
  std::string my_method = method;
  std::transform(my_method.begin(), my_method.end(), my_method.begin(), ::toupper);
  if (my_method == "POST") {
    curl_easy_setopt(curl, CURLOPT_POST, 1);
  } else if (my_method == "PUT") {
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
  } else if (my_method == "GET") {
  } else {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, my_method.c_str());
  }
}

void InvokeHTTP::initialize() {
  logger_->log_info("Initializing InvokeHTTP");

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Method);
  properties.insert(URL);
  properties.insert(ConnectTimeout);
  properties.insert(ReadTimeout);
  properties.insert(DateHeader);
  properties.insert(AttributesToSend);
  properties.insert(SSLContext);
  properties.insert(ProxyHost);
  properties.insert(ProxyPort);
  properties.insert(ProxyUser);
  properties.insert(ProxyPassword);
  properties.insert(ContentType);
  properties.insert(SendBody);
  properties.insert(AlwaysOutputResponse);

  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void InvokeHTTP::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  if (!context->getProperty(Method.getName(), method_)) {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", Method.getName().c_str(), Method.getValue().c_str());
    return;
  }

  if (!context->getProperty(URL.getName(), url_)) {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", URL.getName().c_str(), URL.getValue().c_str());
    return;
  }

  std::string timeoutStr;

  if (context->getProperty(ConnectTimeout.getName(), timeoutStr)) {
    core::Property::StringToInt(timeoutStr, connect_timeout_);
    // set the timeout in curl options.

  } else {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", ConnectTimeout.getName().c_str(), ConnectTimeout.getValue().c_str());

    return;
  }

  if (context->getProperty(ReadTimeout.getName(), timeoutStr)) {
    core::Property::StringToInt(timeoutStr, read_timeout_);

  } else {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", ReadTimeout.getName().c_str(), ReadTimeout.getValue().c_str());
  }

  std::string dateHeaderStr;
  if (!context->getProperty(DateHeader.getName(), dateHeaderStr)) {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", DateHeader.getName().c_str(), DateHeader.getValue().c_str());
  }

  date_header_include_ = utils::StringUtils::StringToBool(dateHeaderStr, date_header_include_);

  if (!context->getProperty(PropPutOutputAttributes.getName(), put_attribute_name_)) {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", PropPutOutputAttributes.getName().c_str(), PropPutOutputAttributes.getValue().c_str());
  }

  if (!context->getProperty(AttributesToSend.getName(), attribute_to_send_regex_)) {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", AttributesToSend.getName().c_str(), AttributesToSend.getValue().c_str());
  }

  std::string always_output_response = "false";
  if (!context->getProperty(AlwaysOutputResponse.getName(), always_output_response)) {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", AttributesToSend.getName().c_str(), AttributesToSend.getValue().c_str());
  }

  utils::StringUtils::StringToBool(always_output_response, always_output_response_);

  std::string penalize_no_retry = "false";
  if (!context->getProperty(PenalizeOnNoRetry.getName(), penalize_no_retry)) {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", AttributesToSend.getName().c_str(), AttributesToSend.getValue().c_str());
  }

  utils::StringUtils::StringToBool(penalize_no_retry, penalize_no_retry_);

  std::string context_name;
  if (context->getProperty(SSLContext.getName(), context_name) && !IsNullOrEmpty(context_name)) {
    std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(context_name);
    if (nullptr != service) {
      ssl_context_service_ = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
    }
  }
}

InvokeHTTP::~InvokeHTTP() {
  curl_global_cleanup();
}

inline bool InvokeHTTP::matches(const std::string &value, const std::string &sregex) {
  if (sregex == ".*")
    return true;

  regex_t regex;
  int ret = regcomp(&regex, sregex.c_str(), 0);
  if (ret)
    return false;
  ret = regexec(&regex, value.c_str(), (size_t) 0, NULL, 0);
  regfree(&regex);
  if (ret)
    return false;

  return true;
}

std::string InvokeHTTP::generateId() {
  uuid_t txId;
  uuid_generate(txId);
  char uuidStr[37];
  uuid_unparse_lower(txId, uuidStr);
  return uuidStr;
}

bool InvokeHTTP::emitFlowFile(const std::string &method) {
  return ("POST" == method || "PUT" == method || "PATCH" == method);
}

struct curl_slist *InvokeHTTP::build_header_list(CURL *curl, std::string regex, const std::map<std::string, std::string> &attributes) {
  struct curl_slist *list = NULL;
  if (curl) {
    for (auto attribute : attributes) {
      if (matches(attribute.first, regex)) {
        std::string attr = attribute.first + ":" + attribute.second;
        list = curl_slist_append(list, attr.c_str());
      }
    }
  }
  return list;
}

bool InvokeHTTP::isSecure(const std::string &url) {
  if (url.find("https") != std::string::npos) {
    return true;
  }
  return false;
}

CURLcode InvokeHTTP::configure_ssl_context(CURL *curl, void *ctx, void *param) {
  minifi::controllers::SSLContextService *ssl_context_service = static_cast<minifi::controllers::SSLContextService*>(param);
  if (!ssl_context_service->configure_ssl_context(static_cast<SSL_CTX*>(ctx))) {
    return CURLE_FAILED_INIT;
  }
  return CURLE_OK;
}

void InvokeHTTP::configure_secure_connection(CURL *http_session) {
  logger_->log_debug("InvokeHTTP -- Using certificate file %s", ssl_context_service_->getCertificateFile());
  curl_easy_setopt(http_session, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(http_session, CURLOPT_SSL_CTX_FUNCTION, &InvokeHTTP::configure_ssl_context);
  curl_easy_setopt(http_session, CURLOPT_SSL_CTX_DATA, static_cast<void*>(ssl_context_service_.get()));
}

void InvokeHTTP::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->get());

  logger_->log_info("onTrigger InvokeHTTP with  %s", method_.c_str());

  if (flowFile == nullptr) {
    if (!emitFlowFile(method_)) {
      logger_->log_info("InvokeHTTP -- create flow file with  %s", method_.c_str());
      flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
    } else {
      logger_->log_info("exiting because method is %s", method_.c_str());
      return;
    }
  } else {
    logger_->log_info("InvokeHTTP -- Received flowfile ");
  }
  // create a transaction id
  std::string tx_id = generateId();

  CURL *http_session = curl_easy_init();
  // set the HTTP request method from libCURL
  set_request_method(http_session, method_);
  if (isSecure(url_) && ssl_context_service_ != nullptr) {
    configure_secure_connection(http_session);
  }

  curl_easy_setopt(http_session, CURLOPT_URL, url_.c_str());

  if (connect_timeout_ > 0) {
    curl_easy_setopt(http_session, CURLOPT_TIMEOUT, connect_timeout_);
  }

  if (read_timeout_ > 0) {
    curl_easy_setopt(http_session, CURLOPT_TIMEOUT, read_timeout_);
  }
  utils::HTTPRequestResponse content;
  curl_easy_setopt(http_session, CURLOPT_WRITEFUNCTION,
                   &utils::HTTPRequestResponse::recieve_write);

  curl_easy_setopt(http_session, CURLOPT_WRITEDATA, static_cast<void*>(&content));

  if (emitFlowFile(method_)) {
    logger_->log_info("InvokeHTTP -- reading flowfile");
    std::shared_ptr<ResourceClaim> claim = flowFile->getResourceClaim();
    if (claim) {
      utils::ByteInputCallBack *callback = new utils::ByteInputCallBack();
      session->read(flowFile, callback);
      utils::CallBackPosition *callbackObj = new utils::CallBackPosition;
      callbackObj->ptr = callback;
      callbackObj->pos = 0;
      logger_->log_info("InvokeHTTP -- Setting callback");
      curl_easy_setopt(http_session, CURLOPT_UPLOAD, 1L);
      curl_easy_setopt(http_session, CURLOPT_INFILESIZE_LARGE,
                       (curl_off_t)callback->getBufferSize());
      curl_easy_setopt(http_session, CURLOPT_READFUNCTION,
                       &utils::HTTPRequestResponse::send_write);
      curl_easy_setopt(http_session, CURLOPT_READDATA,
                       static_cast<void*>(callbackObj));
    } else {
      logger_->log_error("InvokeHTTP -- no resource claim");
    }

  } else {
    logger_->log_info("InvokeHTTP -- Not emitting flowfile to HTTP Server");
  }

  // append all headers
  struct curl_slist *headers = build_header_list(http_session, attribute_to_send_regex_, flowFile->getAttributes());
  curl_easy_setopt(http_session, CURLOPT_HTTPHEADER, headers);

  logger_->log_info("InvokeHTTP -- curl performed");
  res = curl_easy_perform(http_session);

  if (res == CURLE_OK) {
    logger_->log_info("InvokeHTTP -- curl successful");

    bool putToAttribute = !IsNullOrEmpty(put_attribute_name_);

    std::string response_body(content.data.begin(), content.data.end());
    int64_t http_code = 0;
    curl_easy_getinfo(http_session, CURLINFO_RESPONSE_CODE, &http_code);
    char *content_type;
    /* ask for the content-type */
    curl_easy_getinfo(http_session, CURLINFO_CONTENT_TYPE, &content_type);

    flowFile->addAttribute(STATUS_CODE, std::to_string(http_code));
    flowFile->addAttribute(STATUS_MESSAGE, response_body);
    flowFile->addAttribute(REQUEST_URL, url_);
    flowFile->addAttribute(TRANSACTION_ID, tx_id);

    bool isSuccess = ((int32_t) (http_code / 100)) == 2 && res != CURLE_ABORTED_BY_CALLBACK;
    bool output_body_to_requestAttr = (!isSuccess || putToAttribute) && flowFile != nullptr;
    bool output_body_to_content = isSuccess && !putToAttribute;
    bool body_empty = IsNullOrEmpty(content.data);

    logger_->log_info("isSuccess: %d", isSuccess);
    std::shared_ptr<FlowFileRecord> response_flow = nullptr;

    if (output_body_to_content) {
      if (flowFile != nullptr) {
        response_flow = std::static_pointer_cast<FlowFileRecord>(session->create(flowFile));
      } else {
        response_flow = std::static_pointer_cast<FlowFileRecord>(session->create());
      }

      std::string ct = content_type;
      response_flow->addKeyedAttribute(MIME_TYPE, ct);
      response_flow->addAttribute(STATUS_CODE, std::to_string(http_code));
      response_flow->addAttribute(STATUS_MESSAGE, response_body);
      response_flow->addAttribute(REQUEST_URL, url_);
      response_flow->addAttribute(TRANSACTION_ID, tx_id);
      io::DataStream stream((const uint8_t*) content.data.data(), content.data.size());
      // need an import from the data stream.
      session->importFrom(stream, response_flow);
    } else {
      logger_->log_info("Cannot output body to content");
      response_flow = std::static_pointer_cast<FlowFileRecord>(session->create());
    }
    route(flowFile, response_flow, session, context, isSuccess, http_code);
  } else {
    logger_->log_error("InvokeHTTP -- curl_easy_perform() failed %s\n", curl_easy_strerror(res));
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(http_session);
}

void InvokeHTTP::route(std::shared_ptr<FlowFileRecord> &request, std::shared_ptr<FlowFileRecord> &response, core::ProcessSession *session, core::ProcessContext *context, bool isSuccess,
                       int statusCode) {
  // check if we should yield the processor
  if (!isSuccess && request == nullptr) {
    context->yield();
  }

  // If the property to output the response flowfile regardless of status code is set then transfer it
  bool responseSent = false;
  if (always_output_response_ && response != nullptr) {
    session->transfer(response, Success);
    responseSent = true;
  }

  // transfer to the correct relationship
  // 2xx -> SUCCESS
  if (isSuccess) {
    // we have two flowfiles to transfer
    if (request != nullptr) {
      session->transfer(request, Success);
    }
    if (response != nullptr && !responseSent) {
      session->transfer(response, Success);
    }

    // 5xx -> RETRY
  } else if (statusCode / 100 == 5) {
    if (request != nullptr) {
      session->penalize(request);
      session->transfer(request, RelRetry);
    }

    // 1xx, 3xx, 4xx -> NO RETRY
  } else {
    if (request != nullptr) {
      if (penalize_no_retry_) {
        session->penalize(request);
      }
      session->transfer(request, RelNoRetry);
    }
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
