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

#ifndef EXTENSIONS_MQTT_PROTOCOL_PAYLOADSERIALIZER_H_
#define EXTENSIONS_MQTT_PROTOCOL_PAYLOADSERIALIZER_H_

#include "core/state/Value.h"
#include "c2/C2Protocol.h"
#include "io/BaseStream.h"
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
  static void serializeValueNode(state::response::ValueNode &value, std::shared_ptr<io::BaseStream> stream) {
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

      } else
        type = 0;
      stream->write(&type, 1);
    } else {
      auto str = base_type->getStringValue();
      type = 4;
      stream->write(&type, 1);
      stream->writeUTF(str);
    }
  }
  static void serialize(uint16_t op, const C2Payload &payload, std::shared_ptr<io::BaseStream> stream) {
    uint8_t st;
    uint32_t size = payload.getNestedPayloads().size();
    stream->write(size);
    for (auto nested_payload : payload.getNestedPayloads()) {
      op = opToInt(nested_payload.getOperation());
      stream->write(op);
      stream->write(&st, 1);
      stream->writeUTF(nested_payload.getLabel());
      stream->writeUTF(nested_payload.getIdentifier());
      const std::vector<C2ContentResponse> &content = nested_payload.getContent();
      size = content.size();
      stream->write(size);
      for (const auto &payload_content : content) {
        stream->writeUTF(payload_content.name);
        size = payload_content.operation_arguments.size();
        stream->write(size);
        for (auto content : payload_content.operation_arguments) {
          stream->writeUTF(content.first);
          serializeValueNode(content.second, stream);
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
      case Operation::ACKNOWLEDGE:
        op = 1;
        break;
      case Operation::HEARTBEAT:
        op = 2;
        break;
      case Operation::RESTART:
        op = 3;
        break;
      case Operation::DESCRIBE:
        op = 4;
        break;
      case Operation::STOP:
        op = 5;
        break;
      case Operation::START:
        op = 6;
        break;
      case Operation::UPDATE:
        op = 7;
        break;
      default:
        op = 2;
        break;
    }
    return op;
  }
  static std::shared_ptr<io::BaseStream> serialize(uint16_t version, const C2Payload &payload) {
    std::shared_ptr<io::BaseStream> stream = std::make_shared<io::BaseStream>();
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
    stream->writeUTF(payload.getLabel());

    stream->writeUTF(payload.getIdentifier());
    const std::vector<C2ContentResponse> &content = payload.getContent();
    uint32_t size = content.size();
    stream->write(size);
    for (const auto &payload_content : content) {
      stream->writeUTF(payload_content.name);
      size = payload_content.operation_arguments.size();
      stream->write(size);
      for (auto content : payload_content.operation_arguments) {
        stream->writeUTF(content.first);
        serializeValueNode(content.second, stream);
      }
    }
    serialize(op, payload, stream);
    return stream;
  }

  static state::response::ValueNode deserializeValueNode(io::BaseStream *stream) {
    uint8_t type = 0;
    stream->read(&type, 1);
    state::response::ValueNode node;
    switch (type) {
      case 1:
        uint32_t thb;
        stream->read(thb);
        node = thb;
        break;
      case 2:
        uint64_t base;
        stream->read(base);
        node = base;
        break;
      case 3:
        stream->read(&type, 1);
        if (type == 1)
          node = true;
        else
          node = false;
        break;
      default:
      case 4:
        std::string str;
        stream->readUTF(str);
        node = str;
    }
    return node;
  }
  static C2Payload deserialize(std::vector<uint8_t> data) {
    C2Payload payload(Operation::HEARTBEAT, state::UpdateState::READ_COMPLETE, true);
    if (deserialize(data, payload)) {
      return payload;
    }
    return C2Payload(Operation::HEARTBEAT, state::UpdateState::READ_ERROR, true);
  }
  /**
   * Deserializes the payloads
   * @param parent payload to deserialize.
   * @param operation of parent payload
   * @param identifier for this payload
   * @param stream base stream in which we will serialize the parent payload.
   */
  static bool deserializePayload(C2Payload &parent, Operation operation, std::string identifier, io::BaseStream *stream) {
    uint32_t payloads = 0;
    stream->read(payloads);
    uint8_t op, st;
    std::string label;
    for (size_t i = 0; i < payloads; i++) {
      stream->read(op);
      stream->read(st);
      stream->readUTF(label);
      stream->readUTF(identifier);
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
        stream->readUTF(content_name);
        content.name = content_name;
        stream->read(args);
        for (uint32_t j = 0; j < args; j++) {
          std::string first, second;
          stream->readUTF(first);
          content.operation_arguments[first] = deserializeValueNode(stream);
        }
        subPayload.addContent(std::move(content));
      }
      deserializePayload(subPayload, operation, identifier, stream);
      parent.addPayload(std::move(subPayload));

    }
    return true;
  }
  static bool deserialize(std::vector<uint8_t> data, C2Payload &payload) {
    io::DataStream dataStream(data.data(), data.size());
    io::BaseStream stream(&dataStream);

    uint8_t op, st = 0;
    uint16_t version = 0;

    std::string identifier, label;
    // read op
    stream.read(op);
    stream.read(version);
    stream.read(st);
    stream.readUTF(label);
    stream.readUTF(identifier);

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
      stream.readUTF(content_name);
      content.name = content_name;
      stream.read(args);
      for (uint32_t j = 0; j < args; j++) {
        std::string first, second;
        stream.readUTF(first);
        //stream.readUTF(second);
        content.operation_arguments[first] = deserializeValueNode(&stream);
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
        return Operation::ACKNOWLEDGE;
      case 2:
        return Operation::HEARTBEAT;
      case 3:
        return Operation::RESTART;
      case 4:
        return Operation::DESCRIBE;
      case 5:
        return Operation::STOP;
      case 6:
        return Operation::START;
      case 7:
        return Operation::UPDATE;
      default:
        return Operation::HEARTBEAT;
        ;
    }

  }
  PayloadSerializer();
  virtual ~PayloadSerializer();
};

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_MQTT_PROTOCOL_PAYLOADSERIALIZER_H_ */
