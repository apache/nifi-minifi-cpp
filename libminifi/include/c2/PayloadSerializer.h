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

#ifndef LIBMINIFI_INCLUDE_C2_PAYLOADSERIALIZER_H_
#define LIBMINIFI_INCLUDE_C2_PAYLOADSERIALIZER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/state/Value.h"
#include "c2/C2Protocol.h"
#include "minifi-cpp/io/OutputStream.h"
#include "minifi-cpp/io/InputStream.h"
#include "io/BufferStream.h"
#include "minifi-cpp/utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

class PayloadSerializer {
 public:
  /**
   * Static function that serializes the value nodes
   */
  static void serializeValueNode(const state::response::ValueNode &value, std::shared_ptr<io::OutputStream> stream) {
    auto base_type = value.getValue();
    if (!base_type) {
      uint8_t type = 0;
      stream->write(&type, 1);
      return;
    }
    uint8_t type = 0x00;
    if (auto sub_type = std::dynamic_pointer_cast<state::response::IntValue>(base_type)) {
      type = 1;
      stream->write(&type, 1);
      uint32_t value = sub_type->getValue();
      stream->write(value);
    } else if (auto sub_type = std::dynamic_pointer_cast<state::response::Int64Value>(base_type)) {
      type = 2;
      stream->write(&type, 1);
      uint64_t value = sub_type->getValue();
      stream->write(value);
    } else if (auto sub_type = std::dynamic_pointer_cast<state::response::BoolValue>(base_type)) {
      type = 3;
      stream->write(&type, 1);
      if (sub_type->getValue()) {
        type = 1;
      } else {
        type = 0;
      }
      stream->write(&type, 1);
    } else {
      auto str = base_type->getStringValue();
      type = 4;
      stream->write(&type, 1);
      stream->write(str);
    }
  }
  static void serialize(uint16_t op, const C2Payload &payload, std::shared_ptr<io::OutputStream> stream) {
    uint8_t st;
    uint32_t size = gsl::narrow<uint32_t>(payload.getNestedPayloads().size());
    stream->write(size);
    for (const auto &nested_payload : payload.getNestedPayloads()) {
      op = opToInt(nested_payload.getOperation());
      stream->write(op);
      stream->write(&st, 1);
      stream->write(nested_payload.getLabel());
      stream->write(nested_payload.getIdentifier());
      const std::vector<C2ContentResponse> &content = nested_payload.getContent();
      size = gsl::narrow<uint32_t>(content.size());
      stream->write(size);
      for (const auto &payload_content : content) {
        stream->write(payload_content.name);
        size = gsl::narrow<uint32_t>(payload_content.operation_arguments.size());
        stream->write(size);
        for (auto content : payload_content.operation_arguments) {
          stream->write(content.first);
          serializeValueNode(*gsl::not_null(content.second.valueNode()), stream);
        }
      }
      if (nested_payload.getNestedPayloads().size() > 0) {
        serialize(op, nested_payload, stream);
      } else {
        size = 0;
        stream->write(size);
      }
    }
  }

  static uint8_t opToInt(const Operation opt) {
    uint8_t op;

    switch (opt) {
      case Operation::acknowledge:
        op = 1;
        break;
      case Operation::heartbeat:
        op = 2;
        break;
      case Operation::restart:
        op = 3;
        break;
      case Operation::describe:
        op = 4;
        break;
      case Operation::stop:
        op = 5;
        break;
      case Operation::start:
        op = 6;
        break;
      case Operation::update:
        op = 7;
        break;
      case Operation::pause:
        op = 8;
        break;
      case Operation::resume:
        op = 9;
        break;
      default:
        op = 2;
        break;
    }
    return op;
  }
  static std::shared_ptr<io::OutputStream> serialize(uint16_t version, const C2Payload &payload) {
    std::shared_ptr<io::OutputStream> stream = std::make_shared<io::BufferStream>();
    uint16_t op = 0;
    uint8_t st = 0;
    op = opToInt(payload.getOperation());
    stream->write(version);
    stream->write(op);
    if (payload.getStatus().getState() == state::UpdateState::NESTED) {
      st = 1;
      stream->write(&st, 1);
    } else {
      st = 0;
      stream->write(&st, 1);
    }
    stream->write(payload.getLabel());

    stream->write(payload.getIdentifier());
    const std::vector<C2ContentResponse> &content = payload.getContent();
    uint32_t size = gsl::narrow<uint32_t>(content.size());
    stream->write(size);
    for (const auto &payload_content : content) {
      stream->write(payload_content.name);
      size = gsl::narrow<uint32_t>(payload_content.operation_arguments.size());
      stream->write(size);
      for (auto content : payload_content.operation_arguments) {
        stream->write(content.first);
        serializeValueNode(*gsl::not_null(content.second.valueNode()), stream);
      }
    }
    serialize(op, payload, stream);
    return stream;
  }

  static state::response::ValueNode deserializeValueNode(io::InputStream *stream) {
    uint8_t type = 0;
    stream->read(type);
    state::response::ValueNode node;
    switch (type) {
      case 1: {
        uint32_t thb = 0;
        stream->read(thb);
        node = thb;
        break;
      }
      case 2: {
        uint64_t base = 0;
        stream->read(base);
        node = base;
        break;
      }
      case 3: {
        stream->read(type);
        if (type == 1)
          node = true;
        else
          node = false;
        break;
      }
      case 4:
      default: {
        std::string str;
        stream->read(str);
        node = str;
      }
    }
    return node;
  }
  static C2Payload deserialize(const std::vector<std::byte>& data) {
    C2Payload payload(Operation::heartbeat, state::UpdateState::READ_COMPLETE);
    if (deserialize(data, payload)) {
      return payload;
    }
    return C2Payload(Operation::heartbeat, state::UpdateState::READ_ERROR);
  }
  /**
   * Deserializes the payloads
   * @param parent payload to deserialize.
   * @param operation of parent payload
   * @param identifier for this payload
   * @param stream base stream in which we will serialize the parent payload.
   */
  static bool deserializePayload(C2Payload &parent, Operation operation, std::string identifier, io::InputStream *stream) {
    uint32_t payloads = 0;
    stream->read(payloads);
    uint8_t op{}, st{};
    std::string label;
    for (size_t i = 0; i < payloads; i++) {
      stream->read(op);
      stream->read(st);
      stream->read(label);
      stream->read(identifier);
      operation = intToOp(op);
      C2Payload subPayload(operation, st == 1 ? state::UpdateState::NESTED : state::UpdateState::READ_COMPLETE);
      subPayload.setIdentifier(identifier);
      subPayload.setLabel(label);
      uint32_t content_size = 0;
      stream->read(content_size);
      for (uint32_t i = 0; i < content_size; i++) {
        std::string content_name;
        uint32_t args = 0;
        C2ContentResponse content(operation);
        stream->read(content_name);
        content.name = content_name;
        stream->read(args);
        for (uint32_t j = 0; j < args; j++) {
          std::string first, second;
          stream->read(first);
          content.operation_arguments[first] = C2Value{deserializeValueNode(stream)};
        }
        subPayload.addContent(std::move(content));
      }
      deserializePayload(subPayload, operation, identifier, stream);
      parent.addPayload(std::move(subPayload));
    }
    return true;
  }
  static bool deserialize(std::vector<std::byte> data, C2Payload &payload) {
    io::BufferStream stream(data);

    uint8_t op = 0, st = 0;
    uint16_t version = 0;

    std::string identifier, label;
    // read op
    stream.read(op);
    stream.read(version);
    stream.read(st);
    stream.read(label);
    stream.read(identifier);

    Operation operation = intToOp(op);

    C2Payload newPayload(operation, st == 1 ? state::UpdateState::NESTED : state::UpdateState::READ_COMPLETE);
    newPayload.setIdentifier(identifier);
    newPayload.setLabel(label);

    uint32_t content_size = 0;
    stream.read(content_size);
    for (size_t i = 0; i < content_size; i++) {
      std::string content_name;
      uint32_t args = 0;
      C2ContentResponse content(operation);
      stream.read(content_name);
      content.name = content_name;
      stream.read(args);
      for (uint32_t j = 0; j < args; j++) {
        std::string first, second;
        stream.read(first);
        // stream.readUTF(second);
        content.operation_arguments[first] = C2Value{deserializeValueNode(&stream)};
      }
      newPayload.addContent(std::move(content));
    }

    deserializePayload(newPayload, operation, identifier, &stream);
    // we're finished
    payload = std::move(newPayload);
    return true;
  }

 private:
  static Operation intToOp(int op) {
    switch (op) {
      case 1:
        return Operation::acknowledge;
      case 2:
        return Operation::heartbeat;
      case 3:
        return Operation::restart;
      case 4:
        return Operation::describe;
      case 5:
        return Operation::stop;
      case 6:
        return Operation::start;
      case 7:
        return Operation::update;
      case 8:
        return Operation::pause;
      case 9:
        return Operation::resume;
      default:
        return Operation::heartbeat;
    }
  }
  PayloadSerializer();
  virtual ~PayloadSerializer();
};

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_C2_PAYLOADSERIALIZER_H_
