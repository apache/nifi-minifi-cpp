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

C2Payload::C2Payload(Operation op, std::string identifier, bool isRaw)
    : C2Payload(op, state::UpdateState::FULLY_APPLIED, std::move(identifier), isRaw) {
}

C2Payload::C2Payload(Operation op, state::UpdateState state, std::string identifier, bool isRaw)
    : state::Update(state::UpdateStatus(state, 0)),
      ident_(std::move(identifier)),
      op_(op),
      raw_(isRaw) {
}


C2Payload::C2Payload(Operation op, bool isRaw)
    : state::Update(state::UpdateStatus(state::UpdateState::INITIATE, 0)),
      op_(op),
      raw_(isRaw) {
}

C2Payload::C2Payload(Operation op, state::UpdateState state, bool isRaw)
    : state::Update(state::UpdateStatus(state, 0)),
      op_(op),
      raw_(isRaw) {
}

void C2Payload::addContent(C2ContentResponse &&content, bool collapsible) {
  if (collapsible) {
    for (auto &existing_content : content_) {
      if (existing_content.name == content.name) {
        existing_content.operation_arguments.insert(content.operation_arguments.begin(), content.operation_arguments.end());
        return;
      }
    }
  }
  content_.push_back(std::move(content));
}

void C2Payload::setRawData(const std::string &data) {
  raw_data_.reserve(raw_data_.size() + data.size());
  raw_data_.insert(std::end(raw_data_), std::begin(data), std::end(data));
}

void C2Payload::setRawData(const std::vector<char> &data) {
  raw_data_.reserve(raw_data_.size() + data.size());
  raw_data_.insert(std::end(raw_data_), std::begin(data), std::end(data));
}

void C2Payload::setRawData(const std::vector<uint8_t> &data) {
  raw_data_.reserve(raw_data_.size() + data.size());
  std::transform(std::begin(data), std::end(data), std::back_inserter(raw_data_), [](uint8_t c) {
    return static_cast<char>(c);
  });
}

void C2Payload::addPayload(C2Payload &&payload) {
  payloads_.push_back(std::move(payload));
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
