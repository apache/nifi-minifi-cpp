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

#include "c2/C2Payload.h"
#include <utility>
#include <vector>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

C2ContentResponse::C2ContentResponse(Operation op)
    : op(op),
      required(false),
      delay(0),
      ttl(-1) {
}

C2ContentResponse::C2ContentResponse(const C2ContentResponse &other)
    : op(other.op),
      required(other.required),
      delay(other.delay),
      ttl(other.ttl),
      name(other.name),
      ident(other.ident),
      operation_arguments(other.operation_arguments) {
}

C2ContentResponse::C2ContentResponse(const C2ContentResponse &&other)
    : op(other.op),
      required(other.required),
      delay(std::move(other.delay)),
      ttl(std::move(other.ttl)),
      ident(std::move(other.ident)),
      name(std::move(other.name)),
      operation_arguments(std::move(other.operation_arguments)) {
}

C2ContentResponse &C2ContentResponse::operator=(const C2ContentResponse &&other) {
  op = other.op;
  required = other.required;
  delay = std::move(other.delay);
  ttl = std::move(other.ttl);
  name = std::move(other.name);
  ident = std::move(other.ident);
  operation_arguments = std::move(other.operation_arguments);
  return *this;
}

C2ContentResponse &C2ContentResponse::operator=(const C2ContentResponse &other) {
  op = other.op;
  required = other.required;
  delay = other.delay;
  ttl = other.ttl;
  name = other.name;
  operation_arguments = other.operation_arguments;
  return *this;
}

C2Payload::C2Payload(Operation op, std::string identifier, bool resp, bool isRaw)
    : state::Update(state::UpdateStatus(state::UpdateState::INITIATE, 0)),
      op_(op),
      raw_(isRaw),
      ident_(identifier),
      isResponse(resp),
      is_container_(false) {
}

C2Payload::C2Payload(Operation op, bool resp, bool isRaw)
    : state::Update(state::UpdateStatus(state::UpdateState::INITIATE, 0)),
      op_(op),
      raw_(isRaw),
      isResponse(resp),
      is_container_(false) {
}

C2Payload::C2Payload(Operation op, state::UpdateState state, bool resp, bool isRaw)
    : state::Update(state::UpdateStatus(state, 0)),
      op_(op),
      raw_(isRaw),
      isResponse(resp),
      is_container_(false) {
}

C2Payload::C2Payload(const C2Payload &other)
    : state::Update(other),
      isResponse(other.isResponse),
      op_(other.op_),
      raw_(other.raw_),
      label_(other.label_),
      ident_(other.ident_),
      raw_data_(other.raw_data_),
      payloads_(other.payloads_),
      content_(other.content_),
      is_container_(other.is_container_) {
}

C2Payload::C2Payload(const C2Payload &&other)
    : state::Update(std::move(other)),
      isResponse(other.isResponse),
      op_(std::move(other.op_)),
      raw_(other.raw_),
      label_(std::move(other.label_)),
      ident_(std::move(other.ident_)),
      raw_data_(std::move(other.raw_data_)),
      payloads_(std::move(other.payloads_)),
      content_(std::move(other.content_)),
      is_container_(std::move(other.is_container_)) {
}

void C2Payload::setIdentifier(const std::string &ident) {
  ident_ = ident;
}

std::string C2Payload::getIdentifier() const {
  return ident_;
}

Operation C2Payload::getOperation() const {
  return op_;
}

bool C2Payload::validate() {
  return true;
}

const std::vector<C2ContentResponse> &C2Payload::getContent() const {
  return content_;
}

void C2Payload::addContent(const C2ContentResponse &&content) {
  for (auto &existing_content : content_) {
    if (existing_content.name == content.name) {
      for (auto subcontent : existing_content.operation_arguments) {
      }

      for (auto subcontent : content.operation_arguments) {
      }

      existing_content.operation_arguments.insert(content.operation_arguments.begin(), content.operation_arguments.end());

      for (auto subcontent : existing_content.operation_arguments) {
      }

      return;
    }
  }
  content_.push_back(std::move(content));
}

bool C2Payload::isRaw() const {
  return raw_;
}

void C2Payload::setRawData(const std::string &data) {
  raw_data_.insert(std::end(raw_data_), std::begin(data), std::end(data));
}

void C2Payload::setRawData(const std::vector<char> &data) {
  raw_data_.insert(std::end(raw_data_), std::begin(data), std::end(data));
}

void C2Payload::setRawData(const std::vector<uint8_t> &data) {
  std::transform(std::begin(data), std::end(data), std::back_inserter(raw_data_),[](uint8_t c){
    return (char)c;
  });
}


std::vector<char> C2Payload::getRawData() const {
  return raw_data_;
}

void C2Payload::addPayload(const C2Payload &&payload) {
  payloads_.push_back(std::move(payload));
}
const std::vector<C2Payload> &C2Payload::getNestedPayloads() const {
  return payloads_;
}

C2Payload &C2Payload::operator=(const C2Payload &&other) {
  state::Update::operator=(std::move(other));
  isResponse = other.isResponse;
  op_ = std::move(other.op_);
  raw_ = other.raw_;
  if (raw_) {
    raw_data_ = std::move(other.raw_data_);
  }
  label_ = std::move(other.label_);
  payloads_ = std::move(other.payloads_);
  content_ = std::move(other.content_);
  is_container_ = std::move(other.is_container_);
  return *this;
}

C2Payload &C2Payload::operator=(const C2Payload &other) {
  state::Update::operator=(other);
  isResponse = other.isResponse;
  op_ = other.op_;
  raw_ = other.raw_;
  if (raw_) {
    raw_data_ = other.raw_data_;
  }
  label_ = other.label_;
  payloads_ = other.payloads_;
  content_ = other.content_;
  is_container_ = other.is_container_;
  return *this;
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
