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

#include "c2/protocols/RESTSender.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

RESTSender::RESTSender(std::string name, uuid_t uuid)
    : C2Protocol(name, uuid),
      logger_(logging::LoggerFactory<Connectable>::getLogger()) {
}

void RESTSender::initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<Configure> &configure) {
  C2Protocol::initialize(controller, configure);
  // base URL when one is not specified.
  if (nullptr != configure) {
    configure->get("c2.rest.url", rest_uri_);
    configure->get("c2.rest.url.ack", ack_uri_);
  }
  logger_->log_info("Submitting to %s", rest_uri_);
}
C2Payload RESTSender::consumePayload(const std::string &url, const C2Payload &payload, Direction direction, bool async) {
  std::string operation_request_str = getOperation(payload);
  std::string outputConfig;
  if (direction == Direction::TRANSMIT) {
    Json::Value json_payload;
    json_payload["operation"] = operation_request_str;
    if (payload.getIdentifier().length() > 0) {
      json_payload["operationid"] = payload.getIdentifier();
    }
    const std::vector<C2ContentResponse> &content = payload.getContent();

    for (const auto &payload_content : content) {
      Json::Value payload_content_values;
      bool use_sub_option = true;
      if (payload_content.op == payload.getOperation()) {
        for (auto content : payload_content.operation_arguments) {
          if (payload_content.operation_arguments.size() == 1 && payload_content.name == content.first) {
            json_payload[payload_content.name] = content.second;
            use_sub_option = false;
          } else {
            payload_content_values[content.first] = content.second;
          }
        }
      }
      if (use_sub_option)
        json_payload[payload_content.name] = payload_content_values;
    }

    for (const auto &nested_payload : payload.getNestedPayloads()) {
      json_payload[nested_payload.getLabel()] = serializeJsonPayload(json_payload, nested_payload);
    }

    Json::StyledWriter writer;
    outputConfig = writer.write(json_payload);
  }

  return std::move(sendPayload(url, direction, payload, outputConfig));
}

C2Payload RESTSender::consumePayload(const C2Payload &payload, Direction direction, bool async) {
  if (payload.getOperation() == ACKNOWLEDGE) {
    return consumePayload(ack_uri_, payload, direction, async);
  }
  return consumePayload(rest_uri_, payload, direction, async);
}

void RESTSender::update(const std::shared_ptr<Configure> &configure) {
  std::string url;
  configure->get("c2.rest.url", url);
  configure->get("c2.rest.url.ack", url);
}

const C2Payload RESTSender::sendPayload(const std::string url, const Direction direction, const C2Payload &payload, const std::string outputConfig) {
  utils::HTTPClient client(url, ssl_context_service_);
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
  } else {
    // we do not need to set the uplaod callback
    // since we are not uploading anything on a get
    client.set_request_method("GET");
  }
  client.setContentType("application/json");
  bool isOkay = client.submit();
  int64_t respCode = client.getResponseCode();

  if (isOkay && respCode) {
    if (payload.isRaw()) {
      C2Payload response_payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true, true);

      response_payload.setRawData(client.getResponseBody());
      return std::move(response_payload);
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
