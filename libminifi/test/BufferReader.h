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

#include <algorithm>
#include <utility>
#include <memory>
#include <vector>

#include "FlowFileRecord.h"
#include "Stream.h"
#include "utils/gsl.h"

class BufferReader : public org::apache::nifi::minifi::InputStreamCallback {
 public:
  explicit BufferReader(std::vector<uint8_t>& buffer) : buffer_(buffer) {}

  size_t write(org::apache::nifi::minifi::io::BaseStream& input, std::size_t len) {
    uint8_t tmpBuffer[4096]{};
    std::size_t remaining_len = len;
    size_t total_read = 0;
    while (remaining_len > 0) {
      const auto ret = input.read(tmpBuffer, std::min(remaining_len, sizeof(tmpBuffer)));
      if (ret == 0) break;
      if (minifi::io::isError(ret)) return ret;
      remaining_len -= ret;
      total_read += ret;
      auto prevSize = buffer_.size();
      buffer_.resize(prevSize + ret);
      std::move(tmpBuffer, tmpBuffer + ret, buffer_.data() + prevSize);
    }
    return total_read;
  }

  int64_t process(const std::shared_ptr<org::apache::nifi::minifi::io::BaseStream>& stream) {
    const auto write_result = write(*stream.get(), stream->size());
    return minifi::io::isError(write_result) ? -1 : gsl::narrow<int64_t>(write_result);
  }

 private:
  std::vector<uint8_t>& buffer_;
};
