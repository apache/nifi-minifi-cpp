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

#include "InvokeHTTP.h"
#ifdef WIN32
#include <regex>
#else
#include <regex.h>
#endif
#include <memory>
#include <cinttypes>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "utils/ByteArrayCallback.h"
#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "core/Resource.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/gsl.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const char *InvokeHTTP::ProcessorName = "InvokeHTTP";
std::string InvokeHTTP::DefaultContentType = "application/octet-stream";

core::Property InvokeHTTP::Method("HTTP Method", "HTTP request method (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS). "
                                  "Arbitrary methods are also supported. Methods other than POST, PUT and PATCH will be sent without a message body.",
                                  "GET");
core::Property InvokeHTTP::URL(
    core::PropertyBuilder::createProperty("Remote URL")->withDescription("Remote URL which will be connected to, including scheme, host, port, path.")->isRequired(false)->supportsExpressionLanguage(
        true)->build());

core::Property InvokeHTTP::ConnectTimeout(
      core::PropertyBuilder::createProperty("Connection Timeout")->withDescription("Max wait time for connection to remote service")->isRequired(false)
         ->withDefaultValue<core::TimePeriodValue>("5 s")->build());

core::Property InvokeHTTP::ReadTimeout(
      core::PropertyBuilder::createProperty("Read Timeout")->withDescription("Max wait time for response from remote service")->isRequired(false)
         ->withDefaultValue<core::TimePeriodValue>("15 s")->build());

core::Property InvokeHTTP::DateHeader(
    core::PropertyBuilder::createProperty("Include Date Header")->withDescription("Include an RFC-2616 Date header in the request.")->isRequired(false)->withDefaultValue<bool>(true)->build());

core::Property InvokeHTTP::FollowRedirects(
  core::PropertyBuilder::createProperty("Follow Redirects")
  ->withDescription("Follow HTTP redirects issued by remote server.")
  ->withDefaultValue<bool>(true)
  ->build());
core::Property InvokeHTTP::AttributesToSend("Attributes to Send", "Regular expression that defines which attributes to send as HTTP"
                                            " headers in the request. If not defined, no attributes are sent as headers.",
                                            "");
core::Property InvokeHTTP::SSLContext(
    core::PropertyBuilder::createProperty("SSL Context Service")->withDescription("The SSL Context Service used to provide client certificate "
                                                                                  "information for TLS/SSL (https) connections.")->isRequired(false)->withExclusiveProperty("Remote URL", "^http:.*$")
        ->asType<minifi::controllers::SSLContextService>()->build());
core::Property InvokeHTTP::ProxyHost("Proxy Host", "The fully qualified hostname or IP address of the proxy server", "");
core::Property InvokeHTTP::ProxyPort(
    core::PropertyBuilder::createProperty("Proxy Port")->withDescription("The port of the proxy server")
        ->isRequired(false)->build());
core::Property InvokeHTTP::ProxyUsername(
    core::PropertyBuilder::createProperty("invokehttp-proxy-username", "Proxy Username")->withDescription("Username to set when authenticating against proxy")->isRequired(false)->build());
core::Property InvokeHTTP::ProxyPassword(
    core::PropertyBuilder::createProperty("invokehttp-proxy-password", "Proxy Password")->withDescription("Password to set when authenticating against proxy")->isRequired(false)->build());
core::Property InvokeHTTP::ContentType("Content-type", "The Content-Type to specify for when content is being transmitted through a PUT, "
                                       "POST or PATCH. In the case of an empty value after evaluating an expression language expression, "
                                       "Content-Type defaults to",
                                       "application/octet-stream");
core::Property InvokeHTTP::SendBody(
    core::PropertyBuilder::createProperty("send-message-body", "Send Body")
      ->withDescription("DEPRECATED. Only kept for backwards compatibility, no functionality is included.")
      ->withDefaultValue<bool>(true)
      ->build());
core::Property InvokeHTTP::SendMessageBody(
    core::PropertyBuilder::createProperty("Send Message Body")
      ->withDescription("If true, sends the HTTP message body on POST/PUT/PATCH requests (default). "
                        "If false, suppresses the message body and content-type header for these requests.")
      ->withDefaultValue<bool>(true)
      ->build());
core::Property InvokeHTTP::UseChunkedEncoding("Use Chunked Encoding", "When POST'ing, PUT'ing or PATCH'ing content set this property to true in order to not pass the 'Content-length' header"
                                              " and instead send 'Transfer-Encoding' with a value of 'chunked'. This will enable the data transfer mechanism which was introduced in HTTP 1.1 "
                                              "to pass data of unknown lengths in chunks.",
                                              "false");
core::Property InvokeHTTP::PropPutOutputAttributes("Put Response Body in Attribute", "If set, the response body received back will be put into an attribute of the original "
                                                   "FlowFile instead of a separate FlowFile. The attribute key to put to is determined by evaluating value of this property. ",
                                                   "");
core::Property InvokeHTTP::AlwaysOutputResponse("Always Output Response", "Will force a response FlowFile to be generated and routed to the 'Response' relationship "
                                                "regardless of what the server status code received is ",
                                                "false");
core::Property InvokeHTTP::PenalizeOnNoRetry("Penalize on \"No Retry\"", "Enabling this property will penalize FlowFiles that are routed to the \"No Retry\" relationship.", "false");

core::Property InvokeHTTP::DisablePeerVerification("Disable Peer Verification", "Disables peer verification for the SSL session", "false");
const char* InvokeHTTP::STATUS_CODE = "invokehttp.status.code";
const char* InvokeHTTP::STATUS_MESSAGE = "invokehttp.status.message";
const char* InvokeHTTP::RESPONSE_BODY = "invokehttp.response.body";
const char* InvokeHTTP::REQUEST_URL = "invokehttp.request.url";
const char* InvokeHTTP::TRANSACTION_ID = "invokehttp.tx.id";
const char* InvokeHTTP::REMOTE_DN = "invokehttp.remote.dn";
const char* InvokeHTTP::EXCEPTION_CLASS = "invokehttp.java.exception.class";
const char* InvokeHTTP::EXCEPTION_MESSAGE = "invokehttp.java.exception.message";

core::Relationship InvokeHTTP::Success("success", "The original FlowFile will be routed upon success (2xx status codes). "
                                       "It will have new attributes detailing the success of the request.");

core::Relationship InvokeHTTP::RelResponse("response", "A Response FlowFile will be routed upon success (2xx status codes). "
                                           "If the 'Always Output Response' property is true then the response will be sent "
                                           "to this relationship regardless of the status code received.");

core::Relationship InvokeHTTP::RelRetry("retry", "The original FlowFile will be routed on any status code that can be retried "
                                        "(5xx status codes). It will have new attributes detailing the request.");

core::Relationship InvokeHTTP::RelNoRetry("no retry", "The original FlowFile will be routed on any status code that should NOT "
                                          "be retried (1xx, 3xx, 4xx status codes). It will have new attributes detailing the request.");

core::Relationship InvokeHTTP::RelFailure("failure", "The original FlowFile will be routed on any type of connection failure, "
                                          "timeout or general exception. It will have new attributes detailing the request.");

void InvokeHTTP::initialize() {
  logger_->log_trace("Initializing InvokeHTTP");

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
  properties.insert(ProxyUsername);
  properties.insert(ProxyPassword);
  properties.insert(UseChunkedEncoding);
  properties.insert(ContentType);
  properties.insert(SendBody);
  properties.insert(SendMessageBody);
  properties.insert(DisablePeerVerification);
  properties.insert(AlwaysOutputResponse);
  properties.insert(FollowRedirects);

  setSupportedProperties(properties);
  // Set the supported relationships
  setSupportedRelationships({Success, RelResponse, RelFailure, RelRetry, RelNoRetry});
}

bool getTimeMSFromString(const std::string& propertyName, uint64_t& valInt) {
  core::TimeUnit unit;
  return !propertyName.empty()
      && core::Property::StringToTime(propertyName, valInt, unit)
      && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt);
}

void InvokeHTTP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  if (!context->getProperty(Method.getName(), method_)) {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", Method.getName(), Method.getValue());
    return;
  }

  if (!context->getProperty(URL.getName(), url_)) {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", URL.getName(), URL.getValue());
    return;
  }

  uint64_t valInt;
  std::string timeoutStr;
  if (context->getProperty(ConnectTimeout.getName(), timeoutStr)
      && core::Property::getTimeMSFromString(timeoutStr, valInt)) {
    connect_timeout_ms_ =  std::chrono::milliseconds(valInt);
  } else {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", ConnectTimeout.getName(), ConnectTimeout.getValue());
    return;
  }

  std::string contentTypeStr;
  if (context->getProperty(ContentType.getName(), contentTypeStr)) {
    content_type_ = contentTypeStr;
  }

  timeoutStr.clear();
  if (context->getProperty(ReadTimeout.getName(), timeoutStr)
      && core::Property::getTimeMSFromString(timeoutStr, valInt)) {
    read_timeout_ms_ =  std::chrono::milliseconds(valInt);
  } else {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", ReadTimeout.getName(), ReadTimeout.getValue());
  }

  std::string dateHeaderStr;
  if (!context->getProperty(DateHeader.getName(), dateHeaderStr)) {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", DateHeader.getName(), DateHeader.getValue());
  }

  date_header_include_ = utils::StringUtils::toBool(dateHeaderStr).value_or(DateHeader.getValue());

  if (!context->getProperty(PropPutOutputAttributes.getName(), put_attribute_name_)) {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", PropPutOutputAttributes.getName(), PropPutOutputAttributes.getValue());
  }

  if (!context->getProperty(AttributesToSend.getName(), attribute_to_send_regex_)) {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", AttributesToSend.getName(), AttributesToSend.getValue());
  }

  std::string always_output_response;
  if (!context->getProperty(AlwaysOutputResponse.getName(), always_output_response)) {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", AlwaysOutputResponse.getName(), AlwaysOutputResponse.getValue());
  }

  always_output_response_ = utils::StringUtils::toBool(always_output_response).value_or(false);

  std::string penalize_no_retry = "false";
  if (!context->getProperty(PenalizeOnNoRetry.getName(), penalize_no_retry)) {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", PenalizeOnNoRetry.getName(), PenalizeOnNoRetry.getValue());
  }

  penalize_no_retry_ = utils::StringUtils::toBool(penalize_no_retry).value_or(false);

  std::string context_name;
  if (context->getProperty(SSLContext.getName(), context_name) && !IsNullOrEmpty(context_name)) {
    std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(context_name);
    if (nullptr != service) {
      ssl_context_service_ = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
    }
  }

  std::string useChunkedEncoding = "false";
  if (!context->getProperty(UseChunkedEncoding.getName(), useChunkedEncoding)) {
    logger_->log_debug("%s attribute is missing, so default value of %s will be used", UseChunkedEncoding.getName(), UseChunkedEncoding.getValue());
  }

  use_chunked_encoding_ = utils::StringUtils::toBool(useChunkedEncoding).value_or(false);

  std::string disablePeerVerification;
  disable_peer_verification_ = (context->getProperty(DisablePeerVerification.getName(), disablePeerVerification) && utils::StringUtils::toBool(disablePeerVerification).value_or(false));

  proxy_ = {};
  context->getProperty(ProxyHost.getName(), proxy_.host);
  std::string port_str;
  if (context->getProperty(ProxyPort.getName(), port_str) && !port_str.empty()) {
    core::Property::StringToInt(port_str, proxy_.port);
  }
  context->getProperty(ProxyUsername.getName(), proxy_.username);
  context->getProperty(ProxyPassword.getName(), proxy_.password);
  context->getProperty(FollowRedirects.getName(), follow_redirects_);
  context->getProperty(SendMessageBody.getName(), send_body_);
}

InvokeHTTP::~InvokeHTTP() = default;

std::string InvokeHTTP::generateId() {
  return utils::IdGenerator::getIdGenerator()->generate().to_string();
}

bool InvokeHTTP::emitFlowFile(const std::string &method) {
  return ("POST" == method || "PUT" == method || "PATCH" == method);
}

void InvokeHTTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  auto flowFile = session->get();

  std::string url = url_;

  if (flowFile == nullptr) {
    if (!emitFlowFile(method_)) {
      logger_->log_debug("InvokeHTTP -- create flow file with  %s", method_);
      flowFile = session->create();
    } else {
      logger_->log_debug("Exiting because method is %s and there is no flowfile available to execute it, yielding", method_);
      yield();
      return;
    }
  } else {
    logger_->log_debug("InvokeHTTP -- Received flowfile");
  }

  logger_->log_debug("onTrigger InvokeHTTP with %s to %s", method_, url_);

  // create a transaction id
  std::string tx_id = generateId();

  // Note: callback must be declared before callbackObj so that they are destructed in the correct order
  std::unique_ptr<utils::ByteInputCallBack> callback = nullptr;
  std::unique_ptr<utils::HTTPUploadCallback> callbackObj = nullptr;

  // Client declared after the callbacks to make sure the callbacks are still available when the client is destructed
  utils::HTTPClient client(url_, ssl_context_service_);

  client.initialize(method_);
  client.setConnectionTimeout(connect_timeout_ms_);
  client.setReadTimeout(read_timeout_ms_);
  client.setFollowRedirects(follow_redirects_);

  if (send_body_ && !content_type_.empty()) {
    client.setContentType(content_type_);
  }

  if (use_chunked_encoding_) {
    client.setUseChunkedEncoding();
  }

  if (disable_peer_verification_) {
    logger_->log_debug("Disabling peer verification in HTTPClient");
    client.setDisablePeerVerification();
  }

  client.setHTTPProxy(proxy_);

  if (emitFlowFile(method_)) {
    logger_->log_trace("InvokeHTTP -- reading flowfile");
    std::shared_ptr<ResourceClaim> claim = flowFile->getResourceClaim();
    if (claim) {
      callback = std::unique_ptr<utils::ByteInputCallBack>(new utils::ByteInputCallBack());
      if (send_body_) {
        session->read(flowFile, callback.get());
      }
      callbackObj = std::unique_ptr<utils::HTTPUploadCallback>(new utils::HTTPUploadCallback);
      callbackObj->ptr = callback.get();
      callbackObj->pos = 0;
      logger_->log_trace("InvokeHTTP -- Setting callback, size is %d", callback->getBufferSize());
      if (!send_body_) {
        client.appendHeader("Content-Length", "0");
      } else if (!use_chunked_encoding_) {
        client.appendHeader("Content-Length", std::to_string(flowFile->getSize()));
      }
      client.setUploadCallback(callbackObj.get());
      client.setSeekFunction(callbackObj.get());
    } else {
      logger_->log_error("InvokeHTTP -- no resource claim");
    }

  } else {
    logger_->log_trace("InvokeHTTP -- Not emitting flowfile to HTTP Server");
  }

  // append all headers
  client.build_header_list(attribute_to_send_regex_, flowFile->getAttributes());

  logger_->log_trace("InvokeHTTP -- curl performed");
  if (client.submit()) {
    logger_->log_trace("InvokeHTTP -- curl successful");

    bool putToAttribute = !IsNullOrEmpty(put_attribute_name_);

    const std::vector<char> &response_body = client.getResponseBody();
    const std::vector<std::string> &response_headers = client.getHeaders();

    int64_t http_code = client.getResponseCode();
    const char *content_type = client.getContentType();
    flowFile->addAttribute(STATUS_CODE, std::to_string(http_code));
    if (!response_headers.empty())
      flowFile->addAttribute(STATUS_MESSAGE, response_headers.at(0));
    flowFile->addAttribute(REQUEST_URL, url_);
    flowFile->addAttribute(TRANSACTION_ID, tx_id);

    bool isSuccess = (static_cast<int32_t>(http_code / 100) == 2);
    bool output_body_to_content = isSuccess && !putToAttribute;

    logger_->log_debug("isSuccess: %d, response code %" PRId64, isSuccess, http_code);
    std::shared_ptr<core::FlowFile> response_flow = nullptr;

    if (output_body_to_content) {
      if (flowFile != nullptr) {
        response_flow = session->create(flowFile);
      } else {
        response_flow = session->create();
      }

      // if content type isn't returned we should return application/octet-stream
      // as per RFC 2046 -- 4.5.1
      response_flow->addAttribute(core::SpecialFlowAttribute::MIME_TYPE, content_type ? std::string(content_type) : DefaultContentType);
      response_flow->addAttribute(STATUS_CODE, std::to_string(http_code));
      if (!response_headers.empty())
        response_flow->addAttribute(STATUS_MESSAGE, response_headers.at(0));
      response_flow->addAttribute(REQUEST_URL, url);
      response_flow->addAttribute(TRANSACTION_ID, tx_id);
      io::BufferStream stream((const uint8_t*) response_body.data(), gsl::narrow<unsigned int>(response_body.size()));
      // need an import from the data stream.
      session->importFrom(stream, response_flow);
    }
    route(flowFile, response_flow, session, context, isSuccess, http_code);
  } else {
    session->penalize(flowFile);
    session->transfer(flowFile, RelFailure);
  }
}

void InvokeHTTP::route(const std::shared_ptr<core::FlowFile> &request, const std::shared_ptr<core::FlowFile> &response, const std::shared_ptr<core::ProcessSession> &session,
                       const std::shared_ptr<core::ProcessContext> &context, bool isSuccess, int64_t statusCode) {
  // check if we should yield the processor
  if (!isSuccess && request == nullptr) {
    context->yield();
  }

  // If the property to output the response flowfile regardless of status code is set then transfer it
  bool responseSent = false;
  if (always_output_response_ && response != nullptr) {
    logger_->log_debug("Outputting success and response");
    session->transfer(response, RelResponse);
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
      logger_->log_debug("Outputting success and response");
      session->transfer(response, RelResponse);
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

REGISTER_RESOURCE(InvokeHTTP, "An HTTP client processor which can interact with a configurable HTTP Endpoint. "
    "The destination URL and HTTP Method are configurable. FlowFile attributes are converted to HTTP headers and the "
    "FlowFile contents are included as the body of the request (if the HTTP Method is PUT, POST or PATCH).");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
