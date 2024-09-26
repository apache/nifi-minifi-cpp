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

#include <memory>

#include "civetweb.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::io {

class CivetStream : public io::InputStreamImpl {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit CivetStream(struct mg_connection *conn) : conn(conn) {}

  ~CivetStream() override = default;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  size_t read(std::span<std::byte> buf) override {
    const auto ret = mg_read(conn, buf.data(), buf.size());
    if (ret < 0) return STREAM_ERROR;
    return gsl::narrow<size_t>(ret);
  }

 protected:
  struct mg_connection *conn;

 private:
  std::shared_ptr<minifi::core::logging::Logger> logger_;
};
}  // namespace org::apache::nifi::minifi::io
