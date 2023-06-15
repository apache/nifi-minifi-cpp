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

void InvokeHTTP::initialize() {
  logger_->log_trace("Initializing InvokeHTTP");
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
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
  context.getProperty(InvokeHTTP::ProxyHost, proxy.host);
  std::string port_str;
  if (context.getProperty(InvokeHTTP::ProxyPort, port_str) && !port_str.empty()) {
    core::Property::StringToInt(port_str, proxy.port);
  }
  context.getProperty(InvokeHTTP::ProxyUsername, proxy.username);
  context.getProperty(InvokeHTTP::ProxyPassword, proxy.password);

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
  context.getProperty(SendMessageBody, send_message_body_);

  attributes_to_send_ = context.getProperty(AttributesToSend)
                        | utils::filter([](const std::string& s) { return !s.empty(); })  // avoid compiling an empty string to regex
                        | utils::map([](const std::string& regex_str) { return utils::Regex{regex_str}; })
                        | utils::orElse([this] { logger_->log_debug("%s is missing, so the default value will be used", std::string{AttributesToSend.name}); });


  always_output_response_ = context.getProperty<bool>(AlwaysOutputResponse).value_or(false);
  penalize_no_retry_ = context.getProperty<bool>(PenalizeOnNoRetry).value_or(false);

  invalid_http_header_field_handling_strategy_ = utils::parseEnumProperty<invoke_http::InvalidHTTPHeaderFieldHandlingOption>(context, InvalidHTTPHeaderFieldHandlingStrategy);

  put_response_body_in_attribute_ = context.getProperty(PutResponseBodyInAttribute);
  if (put_response_body_in_attribute_ && put_response_body_in_attribute_->empty()) {
    logger_->log_warn("%s is set to an empty string", std::string{PutResponseBodyInAttribute.name});
    put_response_body_in_attribute_.reset();
  }

  use_chunked_encoding_ = context.getProperty<bool>(UseChunkedEncoding).value_or(false);
  send_date_header_ = context.getProperty<bool>(DateHeader).value_or(true);
}

std::unique_ptr<minifi::extensions::curl::HTTPClient> InvokeHTTP::createHTTPClientFromPropertiesAndMembers(const core::ProcessContext& context) const {
  std::string method;
  if (!context.getProperty(Method, method))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Method property missing or invalid");

  std::string url;
  if (!context.getProperty(URL, url))
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
  std::weak_ptr<core::ProcessContext> weak_context = context;
  auto create_client = [this, weak_context]() -> std::unique_ptr<minifi::extensions::curl::HTTPClient> {
    if (auto context = weak_context.lock())
      return createHTTPClientFromPropertiesAndMembers(*context);
    else
      return nullptr;
  };

  client_queue_ = utils::ResourceQueue<extensions::curl::HTTPClient>::create(create_client, getMaxConcurrentTasks(), std::nullopt, logger_);
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
    case invoke_http::InvalidHTTPHeaderFieldHandlingOption::FAIL:
      if (ranges::any_of(matching_attributes, std::not_fn(&extensions::curl::HTTPClient::isValidHttpHeaderField), key_fn)) return false;
      for (const auto& header: matching_attributes) append_header(header.first, header.second);
      return true;
    case invoke_http::InvalidHTTPHeaderFieldHandlingOption::DROP:
      for (const auto& header: matching_attributes | ranges::views::filter(&extensions::curl::HTTPClient::isValidHttpHeaderField, key_fn)) {
        append_header(header.first, header.second);
      }
      return true;
    case invoke_http::InvalidHTTPHeaderFieldHandlingOption::TRANSFORM:
      for (const auto& header: matching_attributes) {
        append_header(extensions::curl::HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName(header.first), header.second);
      }
      return true;
  }
  return true;
}

void InvokeHTTP::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(session && context && client_queue_);

  auto client = client_queue_->getResource();

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

  const auto remove_callback_from_client_at_exit = gsl::finally([&client] {
    client.setUploadCallback({});
  });

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
        client.setPostSize(flow_file->getSize());
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
    flow_file->addAttribute(std::string(STATUS_CODE), std::to_string(http_code));
    if (!response_headers.empty()) { flow_file->addAttribute(std::string(STATUS_MESSAGE), response_headers.at(0)); }
    flow_file->addAttribute(std::string(REQUEST_URL), client.getURL());
    flow_file->addAttribute(std::string(TRANSACTION_ID), transaction_id);

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
        response_flow->addAttribute(std::string(STATUS_CODE), std::to_string(http_code));
        if (!response_headers.empty()) { response_flow->addAttribute(std::string(STATUS_MESSAGE), response_headers.at(0)); }
        response_flow->addAttribute(std::string(REQUEST_URL), client.getURL());
        response_flow->addAttribute(std::string(TRANSACTION_ID), transaction_id);
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
