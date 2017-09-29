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
  CLEAR
};

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
  std::map<std::string, std::string> operation_arguments;
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

  C2Payload(Operation op, std::string identifier, bool resp = false, bool isRaw = false);

  C2Payload(Operation op, bool resp = false, bool isRaw = false);

  C2Payload(Operation op, state::UpdateState state, bool resp = false, bool isRaw = false);

  C2Payload(const C2Payload &other);

  C2Payload(const C2Payload &&other);

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
  void addContent(const C2ContentResponse &&content);

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
   * Returns raw data.
   */
  std::string getRawData() const;

  /**
   * Add a nested payload.
   * @param payload payload to move into this object.
   */
  void addPayload(const C2Payload &&payload);
  /**
   * Get nested payloads.
   */
  const std::vector<C2Payload> &getNestedPayloads() const;

  C2Payload &operator=(const C2Payload &&other);
  C2Payload &operator=(const C2Payload &other);

 protected:

  // identifier for this payload.
  std::string ident_;

  std::string label_;

  std::vector<C2Payload> payloads_;

  std::vector<C2ContentResponse> content_;

  Operation op_;

  bool raw_;

  std::string raw_data_;

  bool isResponse;

};

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_C2PAYLOAD_H_ */
