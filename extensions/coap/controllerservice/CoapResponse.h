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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_COAPRESPONSE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_COAPRESPONSE_H_

#include "coap_message.h"

#include <memory>

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
  explicit CoapResponse(CoapMessage * const msg)
      : code_(msg->code_),
        size_(msg->size_) {
    // we take ownership of data_;
    data_ = std::unique_ptr<uint8_t>(new uint8_t[msg->size_]);
    memcpy(data_.get(), msg->data_, size_);
    free_coap_message(msg);
  }

  explicit CoapResponse(uint32_t code)
      : code_(code),
        size_(0),
        data_(nullptr) {

  }

  CoapResponse(const CoapResponse &other) = delete;

  CoapResponse(CoapResponse &&other) = default;

  ~CoapResponse() {
  }

  /**
   * Retrieve the size of the coap response.
   * @return size_t of size
   */
  size_t getSize() const {
    return size_;
  }

  /**
   * Returns a const pointer to the constant data.
   * @return data pointer.
   */
  const uint8_t * const getData() const {
    return data_.get();
  }

  /**
   * Returns the response code from the data.
   * @return data.
   */
  uint32_t getCode() const {
    return code_;
  }

  /**
   * Ease of use function to take ownership of the CoAPResponse.
   * @param data, data pointer.
   * @size_t size of the data.
   */
  void takeOwnership(uint8_t **data, size_t &size) {
    size = size_;
    *data = data_.release();
  }

  CoapResponse &operator=(const CoapResponse &other) = delete;
  CoapResponse &operator=(CoapResponse &&other) = default;
 private:
  uint32_t code_;
  size_t size_;
  std::unique_ptr<uint8_t> data_;
};

} /* namespace controllers */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CONTROLLERS_COAPRESPONSE_H_ */
