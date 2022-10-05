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

#include "RESTSender.h"

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

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::c2 {

RESTSender::RESTSender(const std::string &name, const utils::Identifier &uuid)
    : C2Protocol(name, uuid) {
}

void RESTSender::initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure) {
  C2Protocol::initialize(controller, configure);
  RESTProtocol::initialize(controller, configure);
  // base URL when one is not specified.
  if (nullptr != configure) {
    std::string update_str;
    std::string ssl_context_service_str;
    configure->get(Configuration::nifi_c2_rest_url, "c2.rest.url", rest_uri_);
    configure->get(Configuration::nifi_c2_rest_url_ack, "c2.rest.url.ack", ack_uri_);
    if (configure->get(Configuration::nifi_c2_rest_ssl_context_service, "c2.rest.ssl.context.service", ssl_context_service_str)) {
      auto service = controller->getControllerService(ssl_context_service_str);
      if (nullptr != service) {
        ssl_context_service_ = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
      }
    }
    if (auto req_encoding_str = configure->get(Configuration::nifi_c2_rest_request_encoding)) {
      if (auto req_encoding = RequestEncoding::parse(req_encoding_str->c_str(), RequestEncoding{}, false)) {
        logger_->log_debug("Using request encoding '%s'", req_encoding.toString());
        req_encoding_ = req_encoding;
      } else {
        logger_->log_error("Invalid request encoding '%s'", req_encoding_str.value());
        req_encoding_ = RequestEncoding::None;
      }
    } else {
      logger_->log_debug("Request encoding is not specified, using default '%s'", toString(RequestEncoding::None));
      req_encoding_ = RequestEncoding::None;
    }
  }
  logger_->log_debug("Submitting to %s", rest_uri_);
}

C2Payload RESTSender::consumePayload(const std::string &url, const C2Payload &payload, Direction direction, bool /*async*/) {
  std::optional<std::string> data;

  if (direction == Direction::TRANSMIT && payload.getOperation() != Operation::TRANSFER) {
    // treat payload as json
    data = serializeJsonRootPayload(payload);
  }
  return sendPayload(url, direction, payload, std::move(data));
}

C2Payload RESTSender::consumePayload(const C2Payload &payload, Direction direction, bool async) {
  if (payload.getOperation() == Operation::ACKNOWLEDGE) {
    return consumePayload(ack_uri_, payload, direction, async);
  }
  return consumePayload(rest_uri_, payload, direction, async);
}

void RESTSender::update(const std::shared_ptr<Configure> &) {
}

void RESTSender::setSecurityContext(extensions::curl::HTTPClient &client, const std::string &type, const std::string &url) {
  // only use the SSL Context if we have a secure URL.
  auto generatedService = std::make_shared<minifi::controllers::SSLContextService>("Service", configuration_);
  generatedService->onEnable();
  client.initialize(type, url, generatedService);
}

C2Payload RESTSender::sendPayload(const std::string& url, const Direction direction, const C2Payload &payload, std::optional<std::string> data) {
  if (url.empty()) {
    return {payload.getOperation(), state::UpdateState::READ_ERROR};
  }

  // Client declared last to make sure callbacks are still available when client is destructed
  extensions::curl::HTTPClient client(url, ssl_context_service_);
  client.setKeepAliveProbe(extensions::curl::KeepAliveProbeData{2s, 2s});
  client.setConnectionTimeout(2s);
  if (direction == Direction::TRANSMIT) {
    client.set_request_method("POST");
    if (!ssl_context_service_ && url.find("https://") == 0) {
      setSecurityContext(client, "POST", url);
    }
    if (payload.getOperation() == Operation::TRANSFER) {
      // treat nested payloads as files
      for (const auto& file : payload.getNestedPayloads()) {
        std::string filename = file.getLabel();
        if (filename.empty()) {
          throw std::logic_error("Missing filename");
        }
        auto file_cb = std::make_unique<utils::HTTPUploadByteArrayInputCallback>();
        file_cb->write(file.getRawDataAsString());
        client.addFormPart("application/octet-stream", "file", std::move(file_cb), filename);
      }
    } else {
      auto data_input = std::make_unique<utils::HTTPUploadByteArrayInputCallback>();
      if (data && req_encoding_ == RequestEncoding::Gzip) {
        io::BufferStream compressed_payload;
        bool compression_success = [&] {
          io::ZlibCompressStream compressor(gsl::make_not_null(&compressed_payload), io::ZlibCompressionFormat::GZIP, Z_BEST_COMPRESSION);
          auto ret = compressor.write(gsl::span<const char>(data.value()).as_span<const std::byte>());
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
    if (!ssl_context_service_ && url.find("https://") == 0) {
      setSecurityContext(client, "GET", url);
    }
    client.set_request_method("GET");
  }

  if (payload.getOperation() == Operation::TRANSFER) {
    auto read = std::make_unique<utils::HTTPReadCallback>(std::numeric_limits<size_t>::max());
    client.setReadCallback(std::move(read));
  } else {
    client.setRequestHeader("Accept", "application/json");
    client.setContentType("application/json");
  }
  bool isOkay = client.submit();
  int64_t respCode = client.getResponseCode();
  const bool clientError = 400 <= respCode && respCode < 500;
  const bool serverError = 500 <= respCode && respCode < 600;
  if (clientError || serverError) {
    logger_->log_error("Error response code '" "%" PRId64 "' from '%s'", respCode, url);
  } else {
    logger_->log_debug("Response code '" "%" PRId64 "' from '%s'", respCode, url);
  }
  const auto response_body_bytes = gsl::make_span(client.getResponseBody()).as_span<const std::byte>();
  logger_->log_trace("Received response: \"%s\"", [&] {return utils::StringUtils::escapeUnprintableBytes(response_body_bytes);});
  if (isOkay && !clientError && !serverError) {
    if (payload.isRaw()) {
      C2Payload response_payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true);
      response_payload.setRawData(response_body_bytes);
      return response_payload;
    }
    return parseJsonResponse(payload, response_body_bytes);
  } else {
    return {payload.getOperation(), state::UpdateState::READ_ERROR};
  }
}

REGISTER_RESOURCE(RESTSender, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::c2
