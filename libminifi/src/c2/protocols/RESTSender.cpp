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

#include "c2/protocols/RESTSender.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <limits>
#include "utils/file/FileUtils.h"
#include "core/Resource.h"
#include "properties/Configuration.h"
#include "io/ZlibStream.h"
#include "controllers/SSLContextService.h"
#include "io/BufferStream.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::c2 {

RESTSender::RESTSender(std::string_view name, const utils::Identifier &uuid)
    : C2Protocol(name, uuid) {
}

void RESTSender::initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure) {
  C2Protocol::initialize(controller, configure);
  RESTProtocol::initialize(controller, configure);
  // base URL when one is not specified.
  if (nullptr != configure) {
    std::optional<std::string> rest_base_path = configure->get(Configuration::nifi_c2_rest_path_base);
    std::string update_str;
    std::string ssl_context_service_str;
    configure->get(Configuration::nifi_c2_rest_url, "c2.rest.url", rest_uri_);
    configure->get(Configuration::nifi_c2_rest_url_ack, "c2.rest.url.ack", ack_uri_);
    if (rest_uri_.starts_with("/")) {
      if (!rest_base_path) {
        throw Exception(ExceptionType::GENERAL_EXCEPTION, "Cannot use relative nifi.c2.rest.url unless the nifi.c2.rest.path.base is set");
      }
      rest_uri_ = rest_base_path.value() + rest_uri_;
    }
    if (ack_uri_.starts_with("/")) {
      if (!rest_base_path) {
        throw Exception(ExceptionType::GENERAL_EXCEPTION, "Cannot use relative nifi.c2.rest.url.ack unless the nifi.c2.rest.path.base is set");
      }
      ack_uri_ = rest_base_path.value() + ack_uri_;
    }
    if (controller && configure->get(Configuration::nifi_c2_rest_ssl_context_service, "c2.rest.ssl.context.service", ssl_context_service_str)) {
      if (auto service = controller->getControllerService(ssl_context_service_str)) {
        ssl_context_service_ = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(service);
      }
    }
    if (nullptr == ssl_context_service_) {
      std::string ssl_context_str;
      if (configure->get(Configure::nifi_remote_input_secure, ssl_context_str) && org::apache::nifi::minifi::utils::string::toBool(ssl_context_str).value_or(false)) {
        ssl_context_service_ = std::make_shared<minifi::controllers::SSLContextServiceImpl>("RESTSenderSSL", configure);
        ssl_context_service_->onEnable();
      }
    }
    if (auto req_encoding_str = configure->get(Configuration::nifi_c2_rest_request_encoding)) {
      if (auto req_encoding = magic_enum::enum_cast<RequestEncoding>(*req_encoding_str, magic_enum::case_insensitive)) {
        logger_->log_debug("Using request encoding '{}'", magic_enum::enum_name(*req_encoding));
        req_encoding_ = *req_encoding;
      } else {
        logger_->log_error("Invalid request encoding '{}'", req_encoding_str.value());
        req_encoding_ = RequestEncoding::none;
      }
    } else {
      logger_->log_debug("Request encoding is not specified, using default '{}'", magic_enum::enum_name(RequestEncoding::none));
      req_encoding_ = RequestEncoding::none;
    }
  }
  logger_->log_debug("Submitting to {}", rest_uri_);
}

C2Payload RESTSender::consumePayload(const std::string &url, const C2Payload &payload, Direction direction, bool /*async*/) {
  std::optional<std::string> data;

  if (direction == Direction::TRANSMIT && payload.getOperation() != Operation::transfer) {
    // treat payload as json
    data = serializeJsonRootPayload(payload);
  }
  return sendPayload(url, direction, payload, std::move(data));
}

C2Payload RESTSender::consumePayload(const C2Payload &payload, Direction direction, bool async) {
  if (payload.getOperation() == Operation::acknowledge) {
    return consumePayload(ack_uri_, payload, direction, async);
  }
  return consumePayload(rest_uri_, payload, direction, async);
}

void RESTSender::update(const std::shared_ptr<Configure> &) {
}

void RESTSender::setSecurityContext(http::HTTPClient &client, http::HttpRequestMethod type, const std::string &url) {
  // only use the SSL Context if we have a secure URL.
  auto generatedService = std::make_shared<minifi::controllers::SSLContextServiceImpl>("Service", configuration_);
  generatedService->onEnable();
  client.initialize(type, url, generatedService);
}

C2Payload RESTSender::sendPayload(const std::string& url, const Direction direction, const C2Payload &payload, std::optional<std::string> data,
                                  const std::optional<std::vector<std::string>>& accepted_formats) {
  if (url.empty()) {
    return {payload.getOperation(), state::UpdateState::READ_ERROR};
  }

  // Client declared last to make sure callbacks are still available when client is destructed
  http::HTTPClient client(url, ssl_context_service_);
  client.setKeepAliveProbe(http::KeepAliveProbeData{2s, 2s});
  client.setConnectionTimeout(2s);

  auto setUpHttpRequest = [&](http::HttpRequestMethod http_method) {
    client.set_request_method(http_method);
    if (url.find("https://") == 0) {
      if (!ssl_context_service_) {
        setSecurityContext(client, http_method, url);
      } else {
        client.initialize(http_method, url, ssl_context_service_);
      }
    }
  };
  if (direction == Direction::TRANSMIT) {
    setUpHttpRequest(http::HttpRequestMethod::POST);
    if (payload.getOperation() == Operation::transfer) {
      // treat nested payloads as files
      for (const auto& file : payload.getNestedPayloads()) {
        std::string filename = file.getLabel();
        if (filename.empty()) {
          throw std::logic_error("Missing filename");
        }
        auto file_cb = std::make_unique<http::HTTPUploadByteArrayInputCallback>();
        file_cb->write(file.getRawDataAsString());
        client.addFormPart("application/octet-stream", "file", std::move(file_cb), filename);
      }
    } else {
      auto data_input = std::make_unique<http::HTTPUploadByteArrayInputCallback>();
      if (data && req_encoding_ == RequestEncoding::gzip) {
        io::BufferStream compressed_payload;
        bool compression_success = [&] {
          io::ZlibCompressStream compressor(gsl::make_not_null(&compressed_payload), io::ZlibCompressionFormat::GZIP, Z_BEST_COMPRESSION);
          auto ret = compressor.write(as_bytes(std::span(data.value())));
          if (ret != data->length()) {
            return false;
          }
          compressor.close();
          return compressor.isFinished();
        }();
        if (compression_success) {
          data_input->setBuffer(compressed_payload.moveBuffer());
          client.setRequestHeader("Content-Encoding", "gzip");
        } else {
          logger_->log_error("Failed to compress request body, falling back to no compression");
          data_input->write(data.value());
        }
      } else {
        data_input->write(data.value_or(""));
      }
      client.setPostSize(data_input->getBufferSize());
      client.setUploadCallback(std::move(data_input));
    }
  } else {
    // we do not need to set the upload callback
    // since we are not uploading anything on a get
    setUpHttpRequest(http::HttpRequestMethod::GET);
  }

  if (payload.getOperation() == Operation::transfer) {
    auto read = std::make_unique<http::HTTPReadCallback>(std::numeric_limits<size_t>::max());
    client.setReadCallback(std::move(read));
    if (accepted_formats && !accepted_formats->empty()) {
      client.setRequestHeader("Accept", utils::string::join(", ", accepted_formats.value()));
    }
  } else {
    // Due to a bug in MiNiFi C2 the Accept header is not handled properly thus we need to exclude it to be compatible
    // TODO(lordgamez): The header should be re-added when the issue in MiNiFi C2 is fixed: https://issues.apache.org/jira/browse/NIFI-10535
    // client.setRequestHeader("Accept", "application/json");
    client.setContentType("application/json");
  }
  bool isOkay = client.submit();
  int64_t respCode = client.getResponseCode();
  const bool clientError = 400 <= respCode && respCode < 500;
  const bool serverError = 500 <= respCode && respCode < 600;
  if (clientError || serverError) {
    logger_->log_error("Error response code '{}' from '{}'", respCode, url);
  } else {
    logger_->log_debug("Response code '{}' from '{}'", respCode, url);
  }
  const auto response_body_bytes = gsl::make_span(client.getResponseBody()).as_span<const std::byte>();
  logger_->log_trace("Received response: \"{}\"", [&] { return utils::string::escapeUnprintableBytes(response_body_bytes); });
  if (isOkay && !clientError && !serverError) {
    if (accepted_formats) {
      C2Payload response_payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true);
      response_payload.setRawData(response_body_bytes);
      return response_payload;
    }
    return parseJsonResponse(payload, response_body_bytes);
  } else {
    return {payload.getOperation(), state::UpdateState::READ_ERROR};
  }
}

C2Payload RESTSender::fetch(const std::string& url, const std::vector<std::string>& accepted_formats, bool /*async*/) {
  return sendPayload(url, Direction::RECEIVE, C2Payload(Operation::transfer, true), std::nullopt, accepted_formats);
}

REGISTER_RESOURCE(RESTSender, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::c2
