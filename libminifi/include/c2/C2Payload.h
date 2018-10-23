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
#ifndef LIBMINIFI_INCLUDE_C2_C2PAYLOAD_H_
#define LIBMINIFI_INCLUDE_C2_C2PAYLOAD_H_

#include <memory>
#include <string>
#include <map>

#include "../core/state/Value.h"
#include "core/state/UpdateController.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

enum Operation {
  ACKNOWLEDGE,
  START,
  STOP,
  RESTART,
  DESCRIBE,
  HEARTBEAT,
  UPDATE,
  VALIDATE,
  CLEAR,
  TRANSFER
};

#define PAYLOAD_NO_STATUS 0
#define PAYLOAD_SUCCESS 1
#define PAYLOAD_FAILURE 2

enum Direction {
  TRANSMIT,
  RECEIVE
};

class C2ContentResponse {
 public:
  C2ContentResponse(Operation op);

  C2ContentResponse(const C2ContentResponse &other);

  C2ContentResponse(const C2ContentResponse &&other);

  C2ContentResponse & operator=(const C2ContentResponse &&other);

  C2ContentResponse & operator=(const C2ContentResponse &other);

  inline bool operator==(const C2ContentResponse &rhs) const {
    if (op != rhs.op)
      return false;
    if (required != rhs.required)
      return false;
    if (ident != rhs.ident)
      return false;
    if (name != rhs.name)
      return false;
    if (operation_arguments != rhs.operation_arguments)
      return false;
    return true;
  }

  inline bool operator!=(const C2ContentResponse &rhs) const {
    return !(*this == rhs);
  }

  Operation op;
  // determines if the operation is required
  bool required;
  // identifier
  std::string ident;
  // delay before running
  uint32_t delay;
  // max time before this response will no longer be honored.
  uint64_t ttl;
  // name applied to commands
  std::string name;
  // commands that correspond with the operation.
  std::map<std::string, state::response::ValueNode> operation_arguments;
//  std::vector<std::string> content;
};

/**
 * C2Payload is an update for the state manager.
 * Note that the payload can either consist of other payloads or
 * have content directly within it, represented by C2ContentResponse objects, above.
 *
 * Payloads can also contain raw data, which can be binary data.
 */
class C2Payload : public state::Update {
 public:
  virtual ~C2Payload() {

  }

  C2Payload(Operation op, const std::string &identifier, bool resp = false, bool isRaw = false);

  C2Payload(Operation op, state::UpdateState state,const std::string &identifier, bool resp = false, bool isRaw = false);

  C2Payload(Operation op, bool resp = false, bool isRaw = false);

  C2Payload(Operation op, state::UpdateState state, bool resp = false, bool isRaw = false);

  C2Payload(const C2Payload &other) = default;

  C2Payload(C2Payload &&other) = default;

  void setIdentifier(const std::string &ident);

  std::string getIdentifier() const;

  void setLabel(const std::string label) {
    label_ = label;
  }

  std::string getLabel() const {
    return label_;
  }

  /**
   * Gets the operation for this payload. May be nested or a single operation.
   */
  Operation getOperation() const;

  /**
   * Validate the payload, if necessary and/or possible.
   */
  virtual bool validate();

  /**
   * Get content responses from this payload.
   */
  const std::vector<C2ContentResponse> &getContent() const;

  /**
   * Add a content response to this payload.
   */
  void addContent(const C2ContentResponse &&content, bool collapsible = true);

  /**
   * Determines if this object contains raw data.
   */
  bool isRaw() const;

  /**
   * Sets raw data within this object.
   */
  void setRawData(const std::string &data);

  /**
   * Sets raw data from a vector within this object.
   */
  void setRawData(const std::vector<char> &data);

  /**
   * Sets raw data from a vector of uint8_t within this object.
   */
  void setRawData(const std::vector<uint8_t> &data);

  /**
   * Returns raw data.
   */
  std::vector<char> getRawData() const;

  /**
   * Add a nested payload.
   * @param payload payload to move into this object.
   */
  void addPayload(const C2Payload &&payload);

  bool isCollapsible() const {
    return is_collapsible_;
  }

  void setCollapsible(bool is_collapsible) {
    is_collapsible_ = is_collapsible;
  }

  bool isContainer() const {
    return is_container_;
  }

  void setContainer(bool is_container) {
    is_container_ = is_container;
  }
  /**
   * Get nested payloads.
   */
  const std::vector<C2Payload> &getNestedPayloads() const;

  C2Payload &operator=(C2Payload &&other) = default;
  C2Payload &operator=(const C2Payload &other) = default;

  inline bool operator==(const C2Payload &rhs) const {
    if (op_ != rhs.op_) {
      return false;
    }
    if (ident_ != rhs.ident_) {
      return false;
    }
    if (label_ != rhs.label_) {
      return false;
    }
    if (payloads_ != rhs.payloads_) {
      return false;
    }
    if (content_ != rhs.content_) {
      return false;
    }
    if (raw_ != rhs.raw_) {
      return false;
    }
    if (raw_data_ != rhs.raw_data_) {
      return false;
    }
    return true;
  }

  inline bool operator!=(const C2Payload &rhs) const {
    return !(*this == rhs);
  }

 protected:

  // identifier for this payload.
  std::string ident_;

  std::string label_;

  std::vector<C2Payload> payloads_;

  std::vector<C2ContentResponse> content_;

  Operation op_;

  bool raw_;

  std::vector<char> raw_data_;

  bool isResponse;

  bool is_container_;

  bool is_collapsible_;

};

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_C2PAYLOAD_H_ */
