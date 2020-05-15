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
#include <limits>

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

struct C2ContentResponse {
  explicit C2ContentResponse(Operation op)
      :op{ op }
  {}

  bool operator==(const C2ContentResponse &rhs) const {
    return op == rhs.op
        && required == rhs.required
        && ident == rhs.ident
        && name == rhs.name
        && operation_arguments == rhs.operation_arguments;
  }

  bool operator!=(const C2ContentResponse &rhs) const { return !(*this == rhs); }

  Operation op;
  // determines if the operation is required
  bool required{ false };
  // identifier
  std::string ident;
  // delay before running
  uint32_t delay{ 0 };
  // max time before this response will no longer be honored.
  uint64_t ttl{ std::numeric_limits<uint64_t>::max() };
  // name applied to commands
  std::string name;
  // commands that correspond with the operation.
  std::map<std::string, state::response::ValueNode> operation_arguments;
};

namespace detail {
template<typename T>
struct is_nothrow_movable : std::integral_constant<bool,
    std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value> {
};

static_assert(is_nothrow_movable<C2ContentResponse>::value, "C2ContentResponse must be nothrow movable");
}  // namespace detail

/**
 * C2Payload is an update for the state manager.
 * Note that the payload can either consist of other payloads or
 * have content directly within it, represented by C2ContentResponse objects, above.
 *
 * Payloads can also contain raw data, which can be binary data.
 */
class C2Payload : public state::Update {
 public:
  C2Payload(Operation op, std::string identifier, bool resp = false, bool isRaw = false);
  C2Payload(Operation op, state::UpdateState state, std::string identifier, bool resp = false, bool isRaw = false);
  explicit C2Payload(Operation op, bool resp = false, bool isRaw = false);
  C2Payload(Operation op, state::UpdateState state, bool resp = false, bool isRaw = false);

  C2Payload(const C2Payload&) = default;
  C2Payload(C2Payload&&) noexcept = default;
  C2Payload &operator=(const C2Payload&) = default;
  C2Payload &operator=(C2Payload&&) noexcept = default;

  ~C2Payload() override = default;

  void setIdentifier(std::string ident) { ident_ = std::move(ident); }
  std::string getIdentifier() const { return ident_; }

  void setLabel(std::string label) { label_ = std::move(label); }
  std::string getLabel() const { return label_; }

  /**
   * Gets the operation for this payload. May be nested or a single operation.
   */
  Operation getOperation() const noexcept { return op_; }

  /**
   * Validate the payload, if necessary and/or possible.
   */
  bool validate() override { return true; }

  /**
   * Get content responses from this payload.
   */
  const std::vector<C2ContentResponse> &getContent() const noexcept { return content_; }

  /**
   * Add a content response to this payload.
   */
  void addContent(C2ContentResponse&&, bool collapsible = true);

  /**
   * Determines if this object contains raw data.
   */
  bool isRaw() const noexcept { return raw_; }

  /**
   * Sets raw data within this object.
   */
  void setRawData(const std::string&);
  void setRawData(const std::vector<char>&);
  void setRawData(const std::vector<uint8_t>&);

  /**
   * Returns raw data.
   */
  std::vector<char> getRawData() const { return raw_data_; }

  /**
   * Add a nested payload.
   * @param payload payload to move into this object.
   */
  void addPayload(C2Payload &&payload);

  bool isCollapsible() const noexcept { return is_collapsible_; }
  void setCollapsible(bool is_collapsible) noexcept { is_collapsible_ = is_collapsible; }

  bool isContainer() const noexcept { return is_container_; }
  void setContainer(bool is_container) noexcept { is_container_ = is_container; }

  /**
   * Get nested payloads.
   */
  const std::vector<C2Payload> &getNestedPayloads() const noexcept { return payloads_; }

  bool operator==(const C2Payload &rhs) const {
    return op_ == rhs.op_
        && ident_ == rhs.ident_
        && label_ == rhs.label_
        && payloads_ == rhs.payloads_
        && content_ == rhs.content_
        && raw_ == rhs.raw_
        && raw_data_ == rhs.raw_data_;
  }

  bool operator!=(const C2Payload &rhs) const { return !(*this == rhs); }

 protected:
  std::string ident_;  // identifier for this payload.
  std::string label_;
  std::vector<C2Payload> payloads_;
  std::vector<C2ContentResponse> content_;
  Operation op_;
  bool raw_{ false };
  std::vector<char> raw_data_;
  bool isResponse{ false };
  bool is_container_{ false };
  bool is_collapsible_{ true };
};

static_assert(detail::is_nothrow_movable<C2Payload>::value, "C2Payload must be nothrow movable");

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_C2PAYLOAD_H_ */
