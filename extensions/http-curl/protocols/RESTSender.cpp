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

#include "RESTSender.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <limits>
#include "utils/file/FileUtils.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

RESTSender::RESTSender(const std::string &name, const utils::Identifier &uuid)
    : C2Protocol(name, uuid),
      logger_(logging::LoggerFactory<RESTSender>::getLogger()) {
}

void RESTSender::initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure) {
  C2Protocol::initialize(controller, configure);
  RESTProtocol::initialize(controller, configure);
  // base URL when one is not specified.
  if (nullptr != configure) {
    std::string update_str, ssl_context_service_str;
    configure->get("nifi.c2.rest.url", "c2.rest.url", rest_uri_);
    configure->get("nifi.c2.rest.url.ack", "c2.rest.url.ack", ack_uri_);
    if (configure->get("nifi.c2.rest.ssl.context.service", "c2.rest.ssl.context.service", ssl_context_service_str)) {
      auto service = controller->getControllerService(ssl_context_service_str);
      if (nullptr != service) {
        ssl_context_service_ = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
      }
    }
  }
  logger_->log_debug("Submitting to %s", rest_uri_);
}

C2Payload RESTSender::consumePayload(const std::string &url, const C2Payload &payload, Direction direction, bool /*async*/) {
  std::string outputConfig;

  if (direction == Direction::TRANSMIT) {
    outputConfig = serializeJsonRootPayload(payload);
  }
  return sendPayload(url, direction, payload, outputConfig);
}

C2Payload RESTSender::consumePayload(const C2Payload &payload, Direction direction, bool async) {
  if (payload.getOperation() == Operation::ACKNOWLEDGE) {
    return consumePayload(ack_uri_, payload, direction, async);
  }
  return consumePayload(rest_uri_, payload, direction, async);
}

void RESTSender::update(const std::shared_ptr<Configure> &configure) {
  std::string url;
  configure->get("nifi.c2.rest.url", "c2.rest.url", url);
  configure->get("nifi.c2.rest.url.ack", "c2.rest.url.ack", url);
}

void RESTSender::setSecurityContext(utils::HTTPClient &client, const std::string &type, const std::string &url) {
  // only use the SSL Context if we have a secure URL.
  auto generatedService = std::make_shared<minifi::controllers::SSLContextService>("Service", configuration_);
  generatedService->onEnable();
  client.initialize(type, url, generatedService);
}

const C2Payload RESTSender::sendPayload(const std::string url, const Direction direction, const C2Payload &payload, const std::string outputConfig) {
  if (url.empty()) {
    return C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
  }

  // Callback for transmit. Declared in order to destruct in proper order - take care!
  std::unique_ptr<utils::ByteInputCallBack> input = nullptr;
  std::unique_ptr<utils::HTTPUploadCallback> callback = nullptr;

  // Callback for transfer. Declared in order to destruct in proper order - take care!
  std::unique_ptr<utils::ByteOutputCallback> file_callback = nullptr;
  utils::HTTPReadCallback read;

  // Client declared last to make sure calbacks are still available when client is destructed
  utils::HTTPClient client(url, ssl_context_service_);
  client.setKeepAliveProbe(std::chrono::milliseconds(2000));
  client.setKeepAliveIdle(std::chrono::milliseconds(2000));
  client.setConnectionTimeout(std::chrono::milliseconds(2000));
  if (direction == Direction::TRANSMIT) {
    input = std::unique_ptr<utils::ByteInputCallBack>(new utils::ByteInputCallBack());
    callback = std::unique_ptr<utils::HTTPUploadCallback>(new utils::HTTPUploadCallback());
    input->write(outputConfig);
    callback->ptr = input.get();
    callback->pos = 0;
    client.set_request_method("POST");
    if (!ssl_context_service_ && url.find("https://") == 0) {
      setSecurityContext(client, "POST", url);
    }
    client.setUploadCallback(callback.get());
    client.setPostSize(outputConfig.size());
  } else {
    // we do not need to set the upload callback
    // since we are not uploading anything on a get
    if (!ssl_context_service_ && url.find("https://") == 0) {
      setSecurityContext(client, "GET", url);
    }
    client.set_request_method("GET");
  }

  if (payload.getOperation() == Operation::TRANSFER) {
    file_callback = std::unique_ptr<utils::ByteOutputCallback>(new utils::ByteOutputCallback(std::numeric_limits<size_t>::max()));
    read.pos = 0;
    read.ptr = file_callback.get();
    client.setReadCallback(&read);
  } else {
    client.appendHeader("Accept: application/json");
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
  auto rs = client.getResponseBody();
  if (isOkay && respCode) {
    if (payload.isRaw()) {
      C2Payload response_payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true);
      response_payload.setRawData(client.getResponseBody());
      return response_payload;
    }
    return parseJsonResponse(payload, client.getResponseBody());
  } else {
    return C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
  }
}

REGISTER_RESOURCE(RESTSender, "Encapsulates the restful protocol that is built upon C2Protocol.");

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
