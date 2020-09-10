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
#include <cstdint>
#include <algorithm>
#include "io/BufferStream.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

int BufferStream::write(const uint8_t *value, int size) {
  gsl_Expects(size >= 0);
  buffer_.reserve(buffer_.size() + size);
  std::copy(value, value + size, std::back_inserter(buffer_));
  return size;
}

int BufferStream::read(uint8_t *buf, int len) {
  gsl_Expects(len >= 0);
  len = (std::min<uint64_t>)(len, buffer_.size() - readOffset_);
  auto begin = buffer_.begin() + readOffset_;
  std::copy(begin, begin + len, buf);

  // increase offset for the next read
  readOffset_ += len;

  return len;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
