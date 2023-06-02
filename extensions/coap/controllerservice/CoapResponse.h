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

#include <cstddef>
#include <memory>
#include <vector>

#include "coap_message.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {
namespace controllers {
/**
 * Purpose and Justification :CoapMessage is in internal message format that is sent to and from consumers of this controller service.
 *
 */
class CoapResponse {
 public:
  /**
   * Creates a CoAPResponse to a CoAPMessage. Takes ownership of the argument
   * and copies the data.
   */
  explicit CoapResponse(CoapMessage* const msg)
      : code_(msg->code_) {
    data_.resize(msg->size_);
    memcpy(data_.data(), msg->data_, data_.size());
    free_coap_message(msg);
  }

  explicit CoapResponse(uint32_t code)
      : code_(code) {
  }

  CoapResponse(const CoapResponse &other) = delete;
  CoapResponse(CoapResponse &&other) = default;
  ~CoapResponse() = default;

  [[nodiscard]] std::span<const std::byte> getData() const noexcept {
    return data_;
  }

  /**
   * Returns the response code from the data.
   * @return data.
   */
  [[nodiscard]] uint32_t getCode() const {
    return code_;
  }

  CoapResponse &operator=(const CoapResponse &other) = delete;
  CoapResponse &operator=(CoapResponse &&other) = default;

 private:
  uint32_t code_;
  std::vector<std::byte> data_;
};

} /* namespace controllers */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
