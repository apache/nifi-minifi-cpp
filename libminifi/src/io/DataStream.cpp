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
#include "io/DataStream.h"
#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <Exception.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

int DataStream::writeData(uint8_t *value, int size) {
  if (value == nullptr)
    return 0;
  std::copy(value, value + size, std::back_inserter(buffer));
  return size;
}

int DataStream::read(uint64_t &value, bool is_little_endian) {
  if ((8 + readBuffer) > buffer.size()) {
    // if read exceed
    return -1;
  }
  uint8_t *buf = &buffer[readBuffer];

  if (is_little_endian) {
    value = ((uint64_t) buf[0] << 56) | ((uint64_t) (buf[1] & 255) << 48) | ((uint64_t) (buf[2] & 255) << 40) | ((uint64_t) (buf[3] & 255) << 32) | ((uint64_t) (buf[4] & 255) << 24)
        | ((uint64_t) (buf[5] & 255) << 16) | ((uint64_t) (buf[6] & 255) << 8) | ((uint64_t) (buf[7] & 255) << 0);
  } else {
    value = ((uint64_t) buf[0] << 0) | ((uint64_t) (buf[1] & 255) << 8) | ((uint64_t) (buf[2] & 255) << 16) | ((uint64_t) (buf[3] & 255) << 24) | ((uint64_t) (buf[4] & 255) << 32)
        | ((uint64_t) (buf[5] & 255) << 40) | ((uint64_t) (buf[6] & 255) << 48) | ((uint64_t) (buf[7] & 255) << 56);
  }
  readBuffer += 8;
  return 8;
}

int DataStream::read(uint32_t &value, bool is_little_endian) {
  if ((4 + readBuffer) > buffer.size()) {
    // if read exceed
    return -1;
  }
  uint8_t *buf = &buffer[readBuffer];

  if (is_little_endian) {
    value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  } else {
    value = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
  }
  readBuffer += 4;
  return 4;
}

int DataStream::read(uint16_t &value, bool is_little_endian) {
  if ((2 + readBuffer) > buffer.size()) {
    // if read exceed
    return -1;
  }
  uint8_t *buf = &buffer[readBuffer];

  if (is_little_endian) {
    value = (buf[0] << 8) | buf[1];
  } else {
    value = buf[0] | buf[1] << 8;
  }
  readBuffer += 2;
  return 2;
}

int DataStream::readData(std::vector<uint8_t> &buf, int buflen) {
  if (buflen < 0) {
    throw minifi::Exception{ExceptionType::GENERAL_EXCEPTION, "negative buflen"};
  }

  if ((buflen + readBuffer) > buffer.size()) {
    // if read exceed
    return -1;
  }

  if (buf.size() < static_cast<size_t>(buflen))
    buf.resize(buflen);

  return this->doReadData(buf.data(), buflen);
}

int DataStream::readData(uint8_t *buf, int buflen) {
  if (buflen < 0) {
    throw minifi::Exception{ ExceptionType::GENERAL_EXCEPTION, "negative buflen" };
  }

  if ((buflen + readBuffer) > buffer.size()) {
    // if read exceed
    return -1;
  }

  return this->doReadData(buf, buflen);
}

int DataStream::doReadData(uint8_t *buf, int buflen) noexcept {
  const auto read_start = std::next(std::begin(buffer), readBuffer);
  const auto read_end = std::next(read_start, buflen);
  const auto write_end = std::copy(read_start, read_end, buf);

  assert(std::distance(buf, write_end) == buflen && "read buflen bytes");
  (void)write_end;

  // increase offset for the next read
  readBuffer += buflen;

  return buflen;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
