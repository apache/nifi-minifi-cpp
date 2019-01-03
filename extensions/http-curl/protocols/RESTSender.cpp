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
#include <iostream>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include <vector>
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"
#include "utils/file/FileManager.h"
#include "utils/FileOutputCallback.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

RESTSender::RESTSender(const std::string &name, const utils::Identifier &uuid)
    : C2Protocol(name, uuid),
      logger_(logging::LoggerFactory<Connectable>::getLogger()) {
}

void RESTSender::initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<Configure> &configure) {
  C2Protocol::initialize(controller, configure);
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
    configure->get("nifi.c2.rest.heartbeat.minimize.updates", "c2.rest.heartbeat.minimize.updates", update_str);
    utils::StringUtils::StringToBool(update_str, minimize_updates_);
  }
  logger_->log_debug("Submitting to %s", rest_uri_);
}

C2Payload RESTSender::consumePayload(const std::string &url, const C2Payload &payload, Direction direction, bool async) {
  std::string outputConfig;

  if (direction == Direction::TRANSMIT) {
    outputConfig = serializeJsonRootPayload(payload);
  }
  return sendPayload(url, direction, payload, outputConfig);
}

C2Payload RESTSender::consumePayload(const C2Payload &payload, Direction direction, bool async) {
  if (payload.getOperation() == ACKNOWLEDGE) {
    return consumePayload(ack_uri_, payload, direction, async);
  }
  return consumePayload(rest_uri_, payload, direction, async);
}

void RESTSender::update(const std::shared_ptr<Configure> &configure) {
  std::string url;
  configure->get("nifi.c2.rest.url", "c2.rest.url", url);
  configure->get("nifi.c2.rest.url.ack", "c2.rest.url.ack", url);
}

const C2Payload RESTSender::sendPayload(const std::string url, const Direction direction, const C2Payload &payload, const std::string outputConfig) {
  if (url.empty()) {
    return C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR, true);
  }
  utils::HTTPClient client(url, ssl_context_service_);
  client.setKeepAliveProbe(2);
  client.setKeepAliveIdle(2);
  client.setConnectionTimeout(2);
  std::unique_ptr<utils::ByteInputCallBack> input = nullptr;
  std::unique_ptr<utils::HTTPUploadCallback> callback = nullptr;
  if (direction == Direction::TRANSMIT) {
    input = std::unique_ptr<utils::ByteInputCallBack>(new utils::ByteInputCallBack());
    callback = std::unique_ptr<utils::HTTPUploadCallback>(new utils::HTTPUploadCallback);
    input->write(outputConfig);
    callback->ptr = input.get();
    callback->pos = 0;
    client.set_request_method("POST");
    client.setUploadCallback(callback.get());
    client.setPostSize(outputConfig.size());
  } else {
    // we do not need to set the uplaod callback
    // since we are not uploading anything on a get
    client.set_request_method("GET");
  }

  std::unique_ptr<utils::FileOutputCallback> file_callback = nullptr;
  utils::HTTPReadCallback read;
  if (payload.getOperation() == TRANSFER) {
    utils::file::FileManager file_man;
    auto file = file_man.unique_file(true);
    file_callback = std::unique_ptr<utils::FileOutputCallback>(new utils::FileOutputCallback(file));
    read.pos = 0;
    read.ptr = file_callback.get();
    client.setReadCallback(&read);
  } else {
    client.appendHeader("Accept: application/json");
    client.setContentType("application/json");
  }
  bool isOkay = client.submit();
  int64_t respCode = client.getResponseCode();
  auto rs = client.getResponseBody();
  if (isOkay && respCode) {
    if (payload.isRaw()) {
      C2Payload response_payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true, true);
      response_payload.setRawData(client.getResponseBody());
      return response_payload;
    }
    return parseJsonResponse(payload, client.getResponseBody());
  } else {
    return C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR, true);
  }
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
