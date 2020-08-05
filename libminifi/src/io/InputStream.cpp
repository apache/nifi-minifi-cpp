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
#include <cstdio>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "io/InputStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

int InputStream::read(uint8_t &value) {
  uint8_t buf = 0;

  if (read(&buf, 1) != 1) {
    return -1;
  }
  value = buf;
  return 1;
}

int InputStream::read(bool &value) {
  uint8_t buf = 0;

  if (read(&buf, 1) != 1) {
    return -1;
  }
  value = buf;
  return 1;
}

int InputStream::read(std::string &str, bool widen) {
  uint32_t len = 0;
  int ret = 0;
  if (!widen) {
    uint16_t shortLength = 0;
    ret = read(shortLength);
    len = shortLength;
  } else {
    ret = read(len);
  }

  if (ret <= 0) {
    return ret;
  }

  if (len == 0) {
    str = "";
    return ret;
  }

  std::vector<uint8_t> buffer(len);
  if (read(buffer.data(), len) != len) {
    return -1;
  }

  str = std::string(reinterpret_cast<const char*>(buffer.data()), len);
  return ret + len;
}

int InputStream::read(uint64_t &value) {
  uint8_t buf[8]{};
  if (read(buf, 8) != 8) {
    return -1;
  }

  value = 0;
  value |= (uint64_t) buf[0] << 56;
  value |= (uint64_t) buf[1] << 48;
  value |= (uint64_t) buf[2] << 40;
  value |= (uint64_t) buf[3] << 32;
  value |= (uint64_t) buf[4] << 24;
  value |= (uint64_t) buf[5] << 16;
  value |= (uint64_t) buf[6] << 8;
  value |= (uint64_t) buf[7];

  return 8;
}

int InputStream::read(uint32_t &value) {
  uint8_t buf[4]{};
  if (read(buf, 4) != 4) {
    return -1;
  }

  value = 0 ;
  value |= (uint32_t) buf[0] << 24;
  value |= (uint32_t) buf[1] << 16;
  value |= (uint32_t) buf[2] << 8;
  value |= (uint32_t) buf[3];

  return 4;
}

int InputStream::read(uint16_t &value) {
  uint8_t buf[2]{};
  if (read(buf, 2) != 2) {
    return -1;
  }

  value = 0;
  value |= (uint16_t) buf[0] << 8;
  value |= (uint16_t) buf[1];

  return 2;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
