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

#include <cinttypes>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/PropertyBuilder.h"
#include "core/Relationship.h"
#include "core/Resource.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/OptionalUtils.h"
#include "range/v3/view/filter.hpp"
#include "range/v3/algorithm/any_of.hpp"

namespace org::apache::nifi::minifi::processors {

std::string InvokeHTTP::DefaultContentType = "application/octet-stream";

const core::Property InvokeHTTP::Method("HTTP Method",
    "HTTP request method (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS). Arbitrary methods are also supported. Methods other than POST, PUT and PATCH will be sent without a message body.",
    "GET");
const core::Property InvokeHTTP::URL(
    core::PropertyBuilder::createProperty("Remote URL")->withDescription("Remote URL which will be connected to, including scheme, host, port, path.")->isRequired(false)->supportsExpressionLanguage(
        true)->build());

const core::Property InvokeHTTP::ConnectTimeout(
    core::PropertyBuilder::createProperty("Connection Timeout")->withDescription("Max wait time for connection to remote service")->isRequired(false)
        ->withDefaultValue<core::TimePeriodValue>("5 s")->build());

const core::Property InvokeHTTP::ReadTimeout(
    core::PropertyBuilder::createProperty("Read Timeout")->withDescription("Max wait time for response from remote service")->isRequired(false)
        ->withDefaultValue<core::TimePeriodValue>("15 s")->build());

const core::Property InvokeHTTP::DateHeader(
    core::PropertyBuilder::createProperty("Include Date Header")->withDescription("Include an RFC-2616 Date header in the request.")->isRequired(false)->withDefaultValue<bool>(true)->build());

const core::Property InvokeHTTP::FollowRedirects(
    core::PropertyBuilder::createProperty("Follow Redirects")
        ->withDescription("Follow HTTP redirects issued by remote server.")
        ->withDefaultValue<bool>(true)
        ->build());
const core::Property InvokeHTTP::AttributesToSend("Attributes to Send",
    "Regular expression that defines which attributes to send as HTTP headers in the request. If not defined, no attributes are sent as headers.",
    "");
const core::Property InvokeHTTP::SSLContext(
    core::PropertyBuilder::createProperty("SSL Context Service")->withDescription(
        "The SSL Context Service used to provide client certificate "
        "information for TLS/SSL (https) connections.")
        ->isRequired(false)
        ->withExclusiveProperty("Remote URL", "^http:.*$")
        ->asType<minifi::controllers::SSLContextService>()->build());
const core::Property InvokeHTTP::ProxyHost("Proxy Host", "The fully qualified hostname or IP address of the proxy server", "");
const core::Property InvokeHTTP::ProxyPort(
    core::PropertyBuilder::createProperty("Proxy Port")->withDescription("The port of the proxy server")
        ->isRequired(false)->build());
const core::Property InvokeHTTP::ProxyUsername(
    core::PropertyBuilder::createProperty("invokehttp-proxy-username", "Proxy Username")->withDescription("Username to set when authenticating against proxy")->isRequired(false)->build());
const core::Property InvokeHTTP::ProxyPassword(
    core::PropertyBuilder::createProperty("invokehttp-proxy-password", "Proxy Password")->withDescription("Password to set when authenticating against proxy")->isRequired(false)->build());
const core::Property InvokeHTTP::ContentType("Content-type",
    "The Content-Type to specify for when content is being transmitted through a PUT, "
    "POST or PATCH. In the case of an empty value after evaluating an expression language expression, "
    "Content-Type defaults to",
    "application/octet-stream");
const core::Property InvokeHTTP::SendBody(
    core::PropertyBuilder::createProperty("send-message-body", "Send Body")
        ->withDescription("DEPRECATED. Only kept for backwards compatibility, no functionality is included.")
        ->withDefaultValue<bool>(true)
        ->build());
const core::Property InvokeHTTP::SendMessageBody(
    core::PropertyBuilder::createProperty("Send Message Body")
        ->withDescription("If true, sends the HTTP message body on POST/PUT/PATCH requests (default). "
                          "If false, suppresses the message body and content-type header for these requests.")
        ->withDefaultValue<bool>(true)
        ->build());
const core::Property InvokeHTTP::UseChunkedEncoding("Use Chunked Encoding",
    "When POST'ing, PUT'ing or PATCH'ing content set this property to true in order to not pass the 'Content-length' header"
    " and instead send 'Transfer-Encoding' with a value of 'chunked'."
    " This will enable the data transfer mechanism which was introduced in HTTP 1.1 to pass data of unknown lengths in chunks.",
    "false");
const core::Property InvokeHTTP::PutResponseBodyInAttribute("Put Response Body in Attribute",
    "If set, the response body received back will be put into an attribute of the original "
    "FlowFile instead of a separate FlowFile. "
    "The attribute key to put to is determined by evaluating value of this property. ",
    "");
const core::Property InvokeHTTP::AlwaysOutputResponse("Always Output Response",
    "Will force a response FlowFile to be generated and routed to the 'Response' relationship regardless of what the server status code received is ",
    "false");
const core::Property InvokeHTTP::PenalizeOnNoRetry("Penalize on \"No Retry\"",
    "Enabling this property will penalize FlowFiles that are routed to the \"No Retry\" relationship.",
    "false");

const core::Property InvokeHTTP::DisablePeerVerification("Disable Peer Verification", "Disables peer verification for the SSL session", "false");

const core::Property InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy(
    core::PropertyBuilder::createProperty("Invalid HTTP Header Field Handling Strategy")
        ->withDescription("Indicates what should happen when an attribute's name is not a valid HTTP header field name. "
                          "Options: transform - invalid characters are replaced, fail - flow file is transferred to failure, drop - drops invalid attributes from HTTP message")
        ->isRequired(true)
        ->withDefaultValue<std::string>(toString(InvalidHTTPHeaderFieldHandlingOption::TRANSFORM))
        ->withAllowableValues<std::string>(InvalidHTTPHeaderFieldHandlingOption::values())
        ->build());


const core::Relationship InvokeHTTP::Success("success",
    "The original FlowFile will be routed upon success (2xx status codes). It will have new attributes detailing the success of the request.");

const core::Relationship InvokeHTTP::RelResponse("response",
    "A Response FlowFile will be routed upon success (2xx status codes). "
    "If the 'Always Output Response' property is true then the response will be sent "
    "to this relationship regardless of the status code received.");

const core::Relationship InvokeHTTP::RelRetry("retry",
    "The original FlowFile will be routed on any status code that can be retried "
    "(5xx status codes). It will have new attributes detailing the request.");

const core::Relationship InvokeHTTP::RelNoRetry("no retry",
    "The original FlowFile will be routed on any status code that should NOT "
    "be retried (1xx, 3xx, 4xx status codes). It will have new attributes detailing the request.");

const core::Relationship InvokeHTTP::RelFailure("failure",
    "The original FlowFile will be routed on any type of connection failure, "
    "timeout or general exception. It will have new attributes detailing the request.");

void InvokeHTTP::initialize() {
  logger_->log_trace("Initializing InvokeHTTP");
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

namespace {
void setupClientTimeouts(extensions::curl::HTTPClient& client, const core::ProcessContext& context) {
  if (auto connection_timeout = context.getProperty<core::TimePeriodValue>(InvokeHTTP::ConnectTimeout))
    client.setConnectionTimeout(connection_timeout->getMilliseconds());

  if (auto read_timeout = context.getProperty<core::TimePeriodValue>(InvokeHTTP::ReadTimeout))
    client.setReadTimeout(read_timeout->getMilliseconds());
}

void setupClientProxy(extensions::curl::HTTPClient& client, const core::ProcessContext& context) {
  utils::HTTPProxy proxy = {};
  context.getProperty(InvokeHTTP::ProxyHost.getName(), proxy.host);
  std::string port_str;
  if (context.getProperty(InvokeHTTP::ProxyPort.getName(), port_str) && !port_str.empty()) {
    core::Property::StringToInt(port_str, proxy.port);
  }
  context.getProperty(InvokeHTTP::ProxyUsername.getName(), proxy.username);
  context.getProperty(InvokeHTTP::ProxyPassword.getName(), proxy.password);

  client.setHTTPProxy(proxy);
}

void setupClientPeerVerification(extensions::curl::HTTPClient& client, const core::ProcessContext& context) {
  if (auto disable_peer_verification = context.getProperty<bool>(InvokeHTTP::DisablePeerVerification))
    client.setPeerVerification(*disable_peer_verification);
}

void setupClientFollowRedirects(extensions::curl::HTTPClient& client, const core::ProcessContext& context) {
  if (auto follow_redirects = context.getProperty<bool>(InvokeHTTP::FollowRedirects))
    client.setFollowRedirects(*follow_redirects);
}

void setupClientContentType(extensions::curl::HTTPClient& client, const core::ProcessContext& context, bool send_body) {
  if (auto content_type = context.getProperty(InvokeHTTP::ContentType)) {
    if (send_body)
      client.setContentType(*content_type);
  }
}

void setupClientTransferEncoding(extensions::curl::HTTPClient& client, bool use_chunked_encoding) {
  if (use_chunked_encoding)
    client.setRequestHeader("Transfer-Encoding", "chunked");
  else
    client.setRequestHeader("Transfer-Encoding", std::nullopt);
}
}  // namespace

void InvokeHTTP::setupMembersFromProperties(const core::ProcessContext& context) {
  context.getProperty(SendMessageBody.getName(), send_message_body_);

  attributes_to_send_ = context.getProperty(AttributesToSend)
                        | utils::filter([](const std::string& s) { return !s.empty(); })  // avoid compiling an empty string to regex
                        | utils::map([](const std::string& regex_str) { return utils::Regex{regex_str}; })
                        | utils::orElse([this] { logger_->log_debug("%s is missing, so the default value will be used", AttributesToSend.getName()); });


  always_output_response_ = context.getProperty<bool>(AlwaysOutputResponse).value_or(false);
  penalize_no_retry_ = context.getProperty<bool>(PenalizeOnNoRetry).value_or(false);

  invalid_http_header_field_handling_strategy_ = utils::parseEnumProperty<InvalidHTTPHeaderFieldHandlingOption>(context, InvalidHTTPHeaderFieldHandlingStrategy);

  put_response_body_in_attribute_ = context.getProperty(PutResponseBodyInAttribute);
  if (put_response_body_in_attribute_ && put_response_body_in_attribute_->empty()) {
    logger_->log_warn("%s is set to an empty string", PutResponseBodyInAttribute.getName());
    put_response_body_in_attribute_.reset();
  }

  use_chunked_encoding_ = context.getProperty<bool>(UseChunkedEncoding).value_or(false);
  send_date_header_ = context.getProperty<bool>(DateHeader).value_or(true);
}

std::unique_ptr<minifi::extensions::curl::HTTPClient> InvokeHTTP::createHTTPClientFromPropertiesAndMembers(const core::ProcessContext& context) const {
  std::string method;
  if (!context.getProperty(Method.getName(), method))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Method property missing or invalid");

  std::string url;
  if (!context.getProperty(URL.getName(), url))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "URL property missing or invalid");

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service;
  if (auto ssl_context_name = context.getProperty(SSLContext)) {
    if (auto service = context.getControllerService(*ssl_context_name)) {
      ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(service);
      if (!ssl_context_service)
        logger_->log_error("Controller service '%s' is not an SSLContextService", *ssl_context_name);
    } else {
      logger_->log_error("Couldn't find controller service with name '%s'", *ssl_context_name);
    }
  }

  auto client = std::make_unique<minifi::extensions::curl::HTTPClient>();
  client->initialize(std::move(method), std::move(url), std::move(ssl_context_service));
  setupClientTimeouts(*client, context);
  setupClientProxy(*client, context);
  setupClientFollowRedirects(*client, context);
  setupClientPeerVerification(*client, context);
  setupClientContentType(*client, context, send_message_body_);
  setupClientTransferEncoding(*client, use_chunked_encoding_);

  return client;
}


void InvokeHTTP::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  gsl_Expects(context);

  setupMembersFromProperties(*context);
  client_queue_ = utils::ResourceQueue<extensions::curl::HTTPClient>::create(getMaxConcurrentTasks(), logger_);
}

bool InvokeHTTP::shouldEmitFlowFile(minifi::extensions::curl::HTTPClient& client) {
  auto method = client.getRequestMethod();
  return ("POST" == method || "PUT" == method || "PATCH" == method);
}

/**
 * Calls append_header with valid HTTP header keys, based on attributes_to_send_
 * @param flow_file
 * @param append_header Callback to append HTTP header to the request
 * @return false when the flow file should be routed to failure, true otherwise
 */
bool InvokeHTTP::appendHeaders(const core::FlowFile& flow_file, /*std::invocable<std::string, std::string>*/ auto append_header) {
  static_assert(std::is_invocable_v<decltype(append_header), std::string, std::string>);
  if (!attributes_to_send_) return true;
  const auto key_fn = [](const std::pair<std::string, std::string>& pair) { return pair.first; };
  const auto original_attributes = flow_file.getAttributes();
  // non-const views, because otherwise it doesn't satisfy viewable_range, and transform would fail
  ranges::viewable_range auto matching_attributes = original_attributes
      | ranges::views::filter([this](const auto& key) { return utils::regexMatch(key, *attributes_to_send_); }, key_fn);
  switch (invalid_http_header_field_handling_strategy_.value()) {
    case InvalidHTTPHeaderFieldHandlingOption::FAIL:
      if (ranges::any_of(matching_attributes, std::not_fn(&extensions::curl::HTTPClient::isValidHttpHeaderField), key_fn)) return false;
      for (const auto& header: matching_attributes) append_header(header.first, header.second);
      return true;
    case InvalidHTTPHeaderFieldHandlingOption::DROP:
      for (const auto& header: matching_attributes | ranges::views::filter(&extensions::curl::HTTPClient::isValidHttpHeaderField, key_fn)) {
        append_header(header.first, header.second);
      }
      return true;
    case InvalidHTTPHeaderFieldHandlingOption::TRANSFORM:
      for (const auto& header: matching_attributes) {
        append_header(extensions::curl::HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName(header.first), header.second);
      }
      return true;
  }
  return true;
}

void InvokeHTTP::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(session && context && client_queue_);
  auto create_client = [&]() -> std::unique_ptr<minifi::extensions::curl::HTTPClient> {
    return createHTTPClientFromPropertiesAndMembers(*context);
  };

  auto client = client_queue_->getResource(create_client);

  onTriggerWithClient(context, session, *client);
}

void InvokeHTTP::onTriggerWithClient(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session,
                                     minifi::extensions::curl::HTTPClient& client) {
  auto flow_file = session->get();

  if (flow_file == nullptr) {
    if (!shouldEmitFlowFile(client)) {
      logger_->log_debug("InvokeHTTP -- create flow file with  %s", client.getRequestMethod());
      flow_file = session->create();
    } else {
      logger_->log_debug("Exiting because method is %s and there is no flowfile available to execute it, yielding", client.getRequestMethod());
      yield();
      return;
    }
  } else {
    logger_->log_debug("InvokeHTTP -- Received flowfile");
  }

  logger_->log_debug("onTrigger InvokeHTTP with %s to %s", client.getRequestMethod(), client.getURL());

  std::string transaction_id = utils::IdGenerator::getIdGenerator()->generate().to_string();

  if (shouldEmitFlowFile(client)) {
    logger_->log_trace("InvokeHTTP -- reading flowfile");
    const auto flow_file_reader_stream = session->getFlowFileContentStream(flow_file);
    if (flow_file_reader_stream) {
      std::unique_ptr<utils::HTTPUploadCallback> callback_obj;
      if (send_message_body_) {
        callback_obj = std::make_unique<utils::HTTPUploadStreamContentsCallback>(flow_file_reader_stream);
      } else {
        callback_obj = std::make_unique<utils::HTTPUploadByteArrayInputCallback>();
      }
      client.setUploadCallback(std::move(callback_obj));
      logger_->log_trace("InvokeHTTP -- Setting callback, size is %d", flow_file->getSize());

      if (!send_message_body_) {
        client.setRequestHeader("Content-Length", "0");
      } else if (!use_chunked_encoding_) {
        client.setRequestHeader("Content-Length", std::to_string(flow_file->getSize()));
      }
    } else {
      logger_->log_error("InvokeHTTP -- no resource claim");
    }
  } else {
    logger_->log_trace("InvokeHTTP -- Not emitting flowfile to HTTP Server");
  }

  if (send_date_header_) {
    auto current_time = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    client.setRequestHeader("Date", utils::timeutils::getRFC2616Format(current_time));
  } else {
    client.setRequestHeader("Date", std::nullopt);
  }

  const auto append_header = [&](const std::string& key, const std::string& value) { client.setRequestHeader(key, value); };
  if (!appendHeaders(*flow_file, append_header)) {
    session->transfer(flow_file, RelFailure);
    return;
  }

  logger_->log_trace("InvokeHTTP -- curl performed");
  if (client.submit()) {
    logger_->log_trace("InvokeHTTP -- curl successful");

    const std::vector<char>& response_body = client.getResponseBody();
    const std::vector<std::string>& response_headers = client.getResponseHeaders();

    int64_t http_code = client.getResponseCode();
    const char* content_type = client.getContentType();
    flow_file->addAttribute(STATUS_CODE, std::to_string(http_code));
    if (!response_headers.empty())
      flow_file->addAttribute(STATUS_MESSAGE, response_headers.at(0));
    flow_file->addAttribute(REQUEST_URL, client.getURL());
    flow_file->addAttribute(TRANSACTION_ID, transaction_id);

    bool is_success = ((http_code / 100) == 2);

    logger_->log_debug("isSuccess: %d, response code %" PRId64, is_success, http_code);
    std::shared_ptr<core::FlowFile> response_flow = nullptr;

    if (is_success) {
      if (!put_response_body_in_attribute_) {
        if (flow_file != nullptr) {
          response_flow = session->create(flow_file);
        } else {
          response_flow = session->create();
        }

        // if content type isn't returned we should return application/octet-stream
        // as per RFC 2046 -- 4.5.1
        response_flow->addAttribute(core::SpecialFlowAttribute::MIME_TYPE, content_type ? std::string(content_type) : DefaultContentType);
        response_flow->addAttribute(STATUS_CODE, std::to_string(http_code));
        if (!response_headers.empty())
          response_flow->addAttribute(STATUS_MESSAGE, response_headers.at(0));
        response_flow->addAttribute(REQUEST_URL, client.getURL());
        response_flow->addAttribute(TRANSACTION_ID, transaction_id);
        io::BufferStream stream(gsl::make_span(response_body).as_span<const std::byte>());
        // need an import from the data stream.
        session->importFrom(stream, response_flow);
      } else {
        if (!response_body.empty()) {
          std::string body_attribute_str{response_body.data(), response_body.size()};
          flow_file->addAttribute(*put_response_body_in_attribute_, body_attribute_str);
        }
      }
    }
    route(flow_file, response_flow, session, context, is_success, http_code);
  } else {
    session->penalize(flow_file);
    session->transfer(flow_file, RelFailure);
  }
}

void InvokeHTTP::route(const std::shared_ptr<core::FlowFile>& request, const std::shared_ptr<core::FlowFile>& response, const std::shared_ptr<core::ProcessSession>& session,
                       const std::shared_ptr<core::ProcessContext>& context, bool is_success, int64_t status_code) {
  // check if we should yield the processor
  if (!is_success && request == nullptr) {
    context->yield();
  }

  // If the property to output the response flowfile regardless of status code is set then transfer it
  bool response_sent = false;
  if (always_output_response_ && response != nullptr) {
    logger_->log_debug("Outputting success and response");
    session->transfer(response, RelResponse);
    response_sent = true;
  }

  // transfer to the correct relationship
  // 2xx -> SUCCESS
  if (is_success) {
    // we have two flowfiles to transfer
    if (request != nullptr) {
      session->transfer(request, Success);
    }
    if (response != nullptr && !response_sent) {
      logger_->log_debug("Outputting success and response");
      session->transfer(response, RelResponse);
    }
    // 5xx -> RETRY
  } else if (status_code / 100 == 5) {
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

REGISTER_RESOURCE(InvokeHTTP, Processor);

}  // namespace org::apache::nifi::minifi::processors
