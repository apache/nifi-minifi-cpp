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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/Resource.h"
#include "io/BufferStream.h"
#include "range/v3/algorithm/any_of.hpp"
#include "range/v3/view/filter.hpp"
#include "utils/OptionalUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {
namespace invoke_http {

HttpClientStore::HttpClientWrapper HttpClientStore::getClient(const std::string& url) {
  std::unique_lock lock(clients_mutex_);
  const auto it = std::find_if(std::begin(unused_clients_), std::end(unused_clients_), [&url](const auto& client) {
    return client->getURL() == url;
  });
  if (it != std::end(unused_clients_)) {
    used_clients_.splice(used_clients_.end(), unused_clients_, it);
    return {*this, **it};
  }

  if (used_clients_.size() + unused_clients_.size() < max_size_) {
    auto client = create_client_function_(url);
    used_clients_.push_back(std::move(client));
    return {*this, *used_clients_.back()};
  } else {
    cv_.wait(lock, [this] { return !unused_clients_.empty(); });
    auto client = create_client_function_(url);
    unused_clients_.front() = std::move(client);
    used_clients_.splice(used_clients_.end(), unused_clients_, unused_clients_.begin());
    return {*this, *used_clients_.back()};
  }
}

void HttpClientStore::returnClient(http::HTTPClient& client) {
  std::unique_lock lock(clients_mutex_);
  const auto it = std::find_if(std::begin(used_clients_), std::end(used_clients_),
    [&client](const auto& elem) { return &client == elem.get(); });
  if (it == std::end(used_clients_)) {
    logger_->log_error("Couldn't find HTTP client in client store to be returned");
    return;
  }
  unused_clients_.splice(unused_clients_.end(), used_clients_, it);
  lock.unlock();
  cv_.notify_one();
}

}  // namespace invoke_http

namespace {
nonstd::expected<std::string_view, std::error_code> removePerSecSuffix(const std::string_view input) {
  const auto trimmed_input = utils::string::trim(input);
  if (trimmed_input.ends_with("/s") || trimmed_input.ends_with("/S")) {
    return trimmed_input.substr(0, trimmed_input.size() - 2);
  }
  return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
}
}  // namespace

nonstd::expected<uint64_t, std::error_code> invoke_http::parseDataTransferSpeed(const std::string_view input) {
  return removePerSecSuffix(input) | utils::andThen(parsing::parseDataSize);
}

bool invoke_http::DataTransferSpeedValidator::validate(const std::string_view input) const {
  return parseDataTransferSpeed(input).has_value();
}


std::string InvokeHTTP::DefaultContentType = "application/octet-stream";

void InvokeHTTP::initialize() {
  logger_->log_trace("Initializing InvokeHTTP");
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

namespace {
void setupClientTimeouts(http::HTTPClient& client,
    std::optional<std::chrono::milliseconds> connection_timeout,
    std::optional<std::chrono::milliseconds> read_timeout) {
  if (connection_timeout)
    client.setConnectionTimeout(*connection_timeout);

  if (read_timeout)
    client.setReadTimeout(*read_timeout);
}

void setupClientTransferEncoding(http::HTTPClient& client, bool use_chunked_encoding) {
  if (use_chunked_encoding)
    client.setRequestHeader("Transfer-Encoding", "chunked");
  else
    client.setRequestHeader("Transfer-Encoding", std::nullopt);
}
}  // namespace

void InvokeHTTP::setupMembersFromProperties(const core::ProcessContext& context) {
  if (const auto url = context.getProperty(URL.name); !url || url->empty())
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "URL property missing or empty");

  method_ = utils::parseEnumProperty<http::HttpRequestMethod>(context, Method);

  send_message_body_ = utils::parseBoolProperty(context, SendMessageBody);

  attributes_to_send_ = context.getProperty(AttributesToSend)
                        | utils::toOptional()
                        | utils::filter([](const std::string& s) { return !s.empty(); })  // avoid compiling an empty string to regex
                        | utils::transform([](const std::string& regex_str) { return utils::Regex{regex_str}; })
                        | utils::orElse([this] { logger_->log_debug("{} is missing, so the default value will be used", AttributesToSend.name); });

  always_output_response_ = utils::parseOptionalBoolProperty(context, AlwaysOutputResponse).value_or(false);
  penalize_no_retry_ = (context.getProperty(PenalizeOnNoRetry) | utils::andThen(parsing::parseBool)).value_or(false);

  invalid_http_header_field_handling_strategy_ = utils::parseEnumProperty<invoke_http::InvalidHTTPHeaderFieldHandlingOption>(context, InvalidHTTPHeaderFieldHandlingStrategy);

  put_response_body_in_attribute_ = context.getProperty(PutResponseBodyInAttribute) | utils::toOptional();
  if (put_response_body_in_attribute_ && put_response_body_in_attribute_->empty()) {
    logger_->log_warn("{} is set to an empty string", PutResponseBodyInAttribute.name);
    put_response_body_in_attribute_.reset();
  }

  use_chunked_encoding_ = utils::parseBoolProperty(context, UseChunkedEncoding);
  send_date_header_ = utils::parseOptionalBoolProperty(context, DateHeader).value_or(true);

  maximum_upload_speed_ = context.getProperty(UploadSpeedLimit) | utils::andThen(invoke_http::parseDataTransferSpeed) | utils::toOptional();
  maximum_download_speed_ = context.getProperty(DownloadSpeedLimit) | utils::andThen(invoke_http::parseDataTransferSpeed) | utils::toOptional();

  connect_timeout_ = utils::parseDurationProperty(context, ConnectTimeout);  // Shouldn't fail due to default value;
  read_timeout_ = utils::parseDurationProperty(context, ReadTimeout);  // Shouldn't fail due to default value;

  proxy_.host = context.getProperty(InvokeHTTP::ProxyHost).value_or("");
  proxy_.port = (context.getProperty(InvokeHTTP::ProxyPort) | utils::andThen(parsing::parseIntegral<int>)).value_or(0);
  std::string port_str;
  proxy_.username = context.getProperty(InvokeHTTP::ProxyUsername).value_or("");
  proxy_.password = context.getProperty(InvokeHTTP::ProxyPassword).value_or("");

  follow_redirects_ = utils::parseBoolProperty(context, FollowRedirects);  // Shouldn't fail due to default value;
  content_type_ = utils::parseProperty(context, InvokeHTTP::ContentType);  // Shouldn't fail due to default value;

  if (auto ssl_context_name = context.getProperty(SSLContext)) {
    if (auto service = context.getControllerService(*ssl_context_name, getUUID())) {
      ssl_context_service_ = std::dynamic_pointer_cast<minifi::controllers::SSLContextServiceInterface>(service);
      if (!ssl_context_service_)
        logger_->log_error("Controller service '{}' is not an SSLContextService", *ssl_context_name);
    } else {
      logger_->log_error("Couldn't find controller service with name '{}'", *ssl_context_name);
    }
  }
}

gsl::not_null<std::unique_ptr<http::HTTPClient>> InvokeHTTP::createHTTPClientFromMembers(const std::string& url) const {
  auto client = std::make_unique<http::HTTPClient>();
  client->initialize(method_, url, ssl_context_service_);
  setupClientTimeouts(*client, connect_timeout_, read_timeout_);
  client->setHTTPProxy(proxy_);
  client->setFollowRedirects(follow_redirects_);
  if (send_message_body_ && content_type_)
    client->setContentType(*content_type_);
  setupClientTransferEncoding(*client, use_chunked_encoding_);
  if (maximum_upload_speed_) {
    client->setMaximumUploadSpeed(*maximum_upload_speed_);
  }
  if (maximum_download_speed_) {
    client->setMaximumDownloadSpeed(*maximum_download_speed_);
  }

  return gsl::make_not_null(std::move(client));
}


void InvokeHTTP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  setupMembersFromProperties(context);

  auto create_client = [this](const std::string& url) -> gsl::not_null<std::unique_ptr<minifi::http::HTTPClient>> {
    return createHTTPClientFromMembers(url);
  };

  client_queue_ = std::make_unique<invoke_http::HttpClientStore>(getMaxConcurrentTasks() * 2, create_client);
}

bool InvokeHTTP::shouldEmitFlowFile() const {
  return (http::HttpRequestMethod::POST == method_ || http::HttpRequestMethod::PUT == method_ || http::HttpRequestMethod::PATCH == method_);
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
  switch (invalid_http_header_field_handling_strategy_) {
    case invoke_http::InvalidHTTPHeaderFieldHandlingOption::fail:
      if (ranges::any_of(matching_attributes, std::not_fn(&http::HTTPClient::isValidHttpHeaderField), key_fn)) return false;
      for (const auto& header: matching_attributes) append_header(header.first, http::HTTPClient::removeInvalidCharactersFromHttpHeaderFieldBody(header.second));
      return true;
    case invoke_http::InvalidHTTPHeaderFieldHandlingOption::drop:
      for (const auto& header: matching_attributes | ranges::views::filter(&http::HTTPClient::isValidHttpHeaderField, key_fn)) {
        append_header(header.first, http::HTTPClient::removeInvalidCharactersFromHttpHeaderFieldBody(header.second));
      }
      return true;
    case invoke_http::InvalidHTTPHeaderFieldHandlingOption::transform:
      for (const auto& header: matching_attributes) {
        append_header(http::HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName(header.first), http::HTTPClient::removeInvalidCharactersFromHttpHeaderFieldBody(header.second));
      }
      return true;
  }
  return true;
}

void InvokeHTTP::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(client_queue_);

  auto flow_file = session.get();

  if (flow_file == nullptr) {
    if (!shouldEmitFlowFile()) {
      logger_->log_debug("InvokeHTTP -- create flow file with  {}", magic_enum::enum_name(method_));
      flow_file = session.create();
    } else {
      logger_->log_debug("Exiting because method is {} and there is no flowfile available to execute it, yielding", magic_enum::enum_name(method_));
      yield();
      return;
    }
  } else {
    logger_->log_debug("InvokeHTTP -- Received flowfile");
  }

  auto url = context.getProperty(URL, flow_file.get());
  if (!url || url->empty()) {
    logger_->log_error("InvokeHTTP -- URL is empty, transferring to failure");
    session.transfer(flow_file, RelFailure);
    return;
  }

  auto client = client_queue_->getClient(*url);
  onTriggerWithClient(context, session, flow_file, client.get());
}

void InvokeHTTP::onTriggerWithClient(core::ProcessContext& context, core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file, minifi::http::HTTPClient& client) {
  logger_->log_debug("onTrigger InvokeHTTP with {} to {}", magic_enum::enum_name(method_), client.getURL());

  const auto remove_callback_from_client_at_exit = gsl::finally([&client] {
    client.setUploadCallback({});
  });

  std::string transaction_id = utils::IdGenerator::getIdGenerator()->generate().to_string();

  if (shouldEmitFlowFile()) {
    logger_->log_trace("InvokeHTTP -- reading flowfile");
    const auto flow_file_reader_stream = session.getFlowFileContentStream(*flow_file);
    if (flow_file_reader_stream) {
      std::unique_ptr<http::HTTPUploadCallback> callback_obj;
      if (send_message_body_) {
        callback_obj = std::make_unique<http::HTTPUploadStreamContentsCallback>(flow_file_reader_stream);
      } else {
        callback_obj = std::make_unique<http::HTTPUploadByteArrayInputCallback>();
      }
      client.setUploadCallback(std::move(callback_obj));
      logger_->log_trace("InvokeHTTP -- Setting callback, size is {}", flow_file->getSize());

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
    session.transfer(flow_file, RelFailure);
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
    if (!response_headers.empty()) { flow_file->addAttribute(STATUS_MESSAGE, utils::string::trim(response_headers.at(0))); }
    flow_file->addAttribute(REQUEST_URL, client.getURL());
    flow_file->addAttribute(TRANSACTION_ID, transaction_id);

    bool is_success = ((http_code / 100) == 2);

    logger_->log_debug("isSuccess: {}, response code {}", is_success, http_code);
    std::shared_ptr<core::FlowFile> response_flow = nullptr;

    if (is_success) {
      if (!put_response_body_in_attribute_) {
        response_flow = session.create(flow_file.get());

        // if content type isn't returned we should return application/octet-stream
        // as per RFC 2046 -- 4.5.1
        response_flow->addAttribute(core::SpecialFlowAttribute::MIME_TYPE, content_type ? std::string(content_type) : DefaultContentType);
        response_flow->addAttribute(STATUS_CODE, std::to_string(http_code));
        if (!response_headers.empty()) { response_flow->addAttribute(STATUS_MESSAGE, utils::string::trim(response_headers.at(0))); }
        response_flow->addAttribute(REQUEST_URL, client.getURL());
        response_flow->addAttribute(TRANSACTION_ID, transaction_id);
        io::BufferStream stream(gsl::make_span(response_body).as_span<const std::byte>());
        // need an import from the data stream.
        session.importFrom(stream, response_flow);
      } else {
        if (!response_body.empty()) {
          std::string body_attribute_str{response_body.data(), response_body.size()};
          flow_file->addAttribute(*put_response_body_in_attribute_, body_attribute_str);
        }
      }
    }
    route(flow_file, response_flow, session, context, is_success, http_code);
  } else {
    session.penalize(flow_file);
    session.transfer(flow_file, RelFailure);
  }
}

void InvokeHTTP::route(const std::shared_ptr<core::FlowFile>& request, const std::shared_ptr<core::FlowFile>& response, core::ProcessSession& session,
                       core::ProcessContext& context, bool is_success, int64_t status_code) {
  // check if we should yield the processor
  if (!is_success && request == nullptr) {
    context.yield();
  }

  // If the property to output the response flowfile regardless of status code is set then transfer it
  bool response_sent = false;
  if (always_output_response_ && response != nullptr) {
    logger_->log_debug("Outputting success and response");
    session.transfer(response, RelResponse);
    response_sent = true;
  }

  // transfer to the correct relationship
  // 2xx -> SUCCESS
  if (is_success) {
    // we have two flowfiles to transfer
    if (request != nullptr) {
      session.transfer(request, Success);
    }
    if (response != nullptr && !response_sent) {
      logger_->log_debug("Outputting success and response");
      session.transfer(response, RelResponse);
    }
    // 5xx -> RETRY
  } else if (status_code / 100 == 5) {
    if (request != nullptr) {
      session.penalize(request);
      session.transfer(request, RelRetry);
    }
    // 1xx, 3xx, 4xx -> NO RETRY
  } else {
    if (request != nullptr) {
      if (penalize_no_retry_) {
        session.penalize(request);
      }
      session.transfer(request, RelNoRetry);
    }
  }
}

REGISTER_RESOURCE(InvokeHTTP, Processor);

}  // namespace org::apache::nifi::minifi::processors
