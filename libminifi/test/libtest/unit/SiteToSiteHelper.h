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

#include <string>
#include <queue>
#include "io/BufferStream.h"
#include "core/Core.h"
#include "utils/gsl.h"

namespace minifi = org::apache::nifi::minifi;

/**
 * Test repository
 */
class SiteToSiteResponder : public minifi::io::BaseStreamImpl {
 private:
  minifi::io::BufferStream server_responses_;
  std::queue<std::string> client_responses_;

 public:
  SiteToSiteResponder() = default;
  // initialize
  int initialize() override {
    return 1;
  }

  void push_response(const std::string& resp) {
    server_responses_.write(reinterpret_cast<const uint8_t*>(resp.data()), resp.length());
  }

  size_t write(const uint8_t *value, size_t size) override {
    client_responses_.push(std::string(reinterpret_cast<const char*>(value), size));
    return size;
  }

  std::string get_next_client_response() {
    std::string ret = client_responses_.front();
    client_responses_.pop();
    return ret;
  }

  /**
   * reads a byte array from the stream
   * @param out_buffer reference in which will set the result
   * @param len length to read
   * @return resulting read size
   **/
  size_t read(std::span<std::byte> out_buffer) override {
    return server_responses_.read(out_buffer);
  }
};
