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
#pragma once

#include <vector>
#include <memory>
#include <utility>
#include <string>
#include <map>
#include <limits>

#include "core/state/Value.h"
#include "core/state/UpdateController.h"
#include "utils/Enum.h"
#include "utils/gsl.h"
#include "utils/span.h"
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::c2 {

enum class Operation : uint8_t {
  acknowledge,
  start,
  stop,
  restart,
  describe,
  heartbeat,
  update,
  clear,
  transfer,
  pause,
  resume,
  sync
};

enum class DescribeOperand : uint8_t {
  metrics,
  configuration,
  manifest,
  jstack,
  corecomponentstate
};

enum class UpdateOperand : uint8_t {
  configuration,
  properties,
  asset
};

enum class TransferOperand : uint8_t {
  debug
};

enum class ClearOperand : uint8_t{
  connection,
  repositories,
  corecomponentstate
};

enum class SyncOperand : uint8_t{
  resource
};

#define PAYLOAD_NO_STATUS 0
#define PAYLOAD_SUCCESS 1
#define PAYLOAD_FAILURE 2

enum Direction {
  TRANSMIT,
  RECEIVE
};

class C2Value {
 public:
  friend std::ostream& operator<<(std::ostream& out, const C2Value& val);

  C2Value() = default;
  C2Value(const C2Value& other) {
    (*this) = other;
  }
  C2Value(C2Value&&) = default;
  template<typename T>
  requires(std::constructible_from<state::response::ValueNode, T>)
  explicit C2Value(T&& value) { value_ = state::response::ValueNode{std::forward<T>(value)}; }
  explicit C2Value(const rapidjson::Value& json_value) {
    value_.emplace<rapidjson::Document>();
    get<rapidjson::Document>(value_).CopyFrom(json_value, get<rapidjson::Document>(value_).GetAllocator());
  }
  explicit C2Value(rapidjson::Document&& json_doc) {
    value_ = std::move(json_doc);
  }

  C2Value& operator=(const C2Value& other) {
    if (auto* other_val_node = get_if<state::response::ValueNode>(&other.value_)) {
      value_ = *other_val_node;
    } else {
      value_.emplace<rapidjson::Document>();
      get<rapidjson::Document>(value_).CopyFrom(get<rapidjson::Document>(other.value_), get<rapidjson::Document>(value_).GetAllocator());
    }
    return *this;
  }

  C2Value& operator=(C2Value&&) = default;


  bool empty() const {
    if (auto* val_node = get_if<state::response::ValueNode>(&value_)) {
      return val_node->empty();
    }
    return false;
  }

  std::string to_string() const {
    if (auto* val_node = get_if<state::response::ValueNode>(&value_)) {
      return val_node->to_string();
    }
    return std::string{get<rapidjson::Document>(value_).GetString(), get<rapidjson::Document>(value_).GetStringLength()};
  }

  const rapidjson::Document* json() const {
    return get_if<rapidjson::Document>(&value_);
  }

  const state::response::ValueNode* valueNode() const {
    return get_if<state::response::ValueNode>(&value_);
  }

  bool operator==(const C2Value&) const = default;

 private:
  std::variant<state::response::ValueNode, rapidjson::Document> value_;
};

struct C2ContentResponse {
  explicit C2ContentResponse(Operation op)
      :op{ op }
  {}

  C2ContentResponse(const C2ContentResponse&) = default;
  C2ContentResponse(C2ContentResponse&&) = default;
  C2ContentResponse& operator=(const C2ContentResponse&) = default;
  C2ContentResponse& operator=(C2ContentResponse&&) = default;

  bool operator==(const C2ContentResponse &other) const {
    return std::tie(this->op, this->required, this->ident, this->name, this->operation_arguments)
        == std::tie(other.op, other.required, other.ident, other.name, other.operation_arguments);
  }

  bool operator!=(const C2ContentResponse &rhs) const { return !(*this == rhs); }

  friend std::ostream& operator<<(std::ostream& out, const C2ContentResponse& response);

  std::optional<std::string> getStringArgument(const std::string& arg_name) const {
    if (auto it = operation_arguments.find(arg_name); it != operation_arguments.end()) {
      return it->second.to_string();
    }
    return std::nullopt;
  }

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
  std::map<std::string, C2Value> operation_arguments;
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
  C2Payload(Operation op, std::string identifier, bool isRaw = false);
  C2Payload(Operation op, state::UpdateState state, std::string identifier, bool isRaw = false);
  explicit C2Payload(Operation op, bool isRaw = false);
  C2Payload(Operation op, state::UpdateState state, bool isRaw = false);

  C2Payload(const C2Payload&) = default;
  C2Payload(C2Payload&&) = default;
  C2Payload &operator=(const C2Payload&) = default;
  C2Payload &operator=(C2Payload&&) = default;

  ~C2Payload() override = default;

  void setIdentifier(std::string ident) { ident_ = std::move(ident); }
  [[nodiscard]]
  std::string getIdentifier() const { return ident_; }

  void setLabel(std::string label) { label_ = std::move(label); }
  [[nodiscard]]
  std::string getLabel() const { return label_; }

  /**
   * Gets the operation for this payload. May be nested or a single operation.
   */
  [[nodiscard]]
  Operation getOperation() const noexcept { return op_; }

  /**
   * Validate the payload, if necessary and/or possible.
   */
  bool validate() override { return true; }

  [[nodiscard]]
  const std::vector<C2ContentResponse> &getContent() const noexcept { return content_; }

  /**
   * Add a content response to this payload.
   */
  void addContent(C2ContentResponse&&, bool collapsible = true);

  /**
   * Determines if this object contains raw data.
   */
  [[nodiscard]]
  bool isRaw() const noexcept { return raw_; }

  /**
   * Sets raw data within this object.
   */
  void setRawData(const std::string&);
  void setRawData(const std::vector<char>&);
  void setRawData(std::span<const std::byte>);

  /**
   * Returns raw data.
   */
  [[nodiscard]] std::vector<std::byte> getRawData() const noexcept { return raw_data_; }
  [[nodiscard]] std::string getRawDataAsString() const {
    const auto raw_data = getRawData();
    return utils::span_to<std::string>(utils::as_span<const char>(std::span(raw_data)));
  }
  [[nodiscard]] std::vector<std::byte> moveRawData() && {return std::move(raw_data_);}

  /**
   * Add a nested payload.
   * @param payload payload to move into this object.
   */
  void addPayload(C2Payload &&payload);

  [[nodiscard]]
  bool isCollapsible() const noexcept { return is_collapsible_; }
  void setCollapsible(bool is_collapsible) noexcept { is_collapsible_ = is_collapsible; }

  [[nodiscard]]
  bool isContainer() const noexcept { return is_container_; }
  void setContainer(bool is_container) noexcept { is_container_ = is_container; }

  [[nodiscard]]
  const std::vector<C2Payload> &getNestedPayloads() const & noexcept { return payloads_; }

  std::vector<C2Payload>&& getNestedPayloads() && noexcept {return std::move(payloads_);}

  void reservePayloads(size_t new_capacity) { payloads_.reserve(new_capacity); }

  bool operator==(const C2Payload &other) const {
    return std::tie(this->op_, this->ident_, this->label_, this->payloads_, this->content_, this->raw_, this->raw_data_)
        == std::tie(other.op_, other.ident_, other.label_, other.payloads_, other.content_, other.raw_, other.raw_data_);
  }

  bool operator!=(const C2Payload &rhs) const { return !(*this == rhs); }

  friend std::ostream& operator<<(std::ostream& out, const C2Payload& payload);

  [[nodiscard]] std::string str() const {
    std::stringstream ss;
    ss << *this;
    return std::move(ss).str();
  }

 protected:
  std::string ident_;  // identifier for this payload.
  std::string label_;
  std::vector<C2Payload> payloads_;
  std::vector<C2ContentResponse> content_;
  Operation op_;
  bool raw_{ false };
  std::vector<std::byte> raw_data_;
  bool is_container_{ false };
  bool is_collapsible_{ true };
};

}  // namespace org::apache::nifi::minifi::c2
