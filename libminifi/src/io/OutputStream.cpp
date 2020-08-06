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
#include "io/OutputStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

int OutputStream::write(const std::vector<uint8_t>& buffer, unsigned int len) {
  if (buffer.size() < len) {
    return -1;
  }
  return write(buffer.data(), len);
}

int OutputStream::write(uint8_t value) {
  return write(&value, 1);
}

int OutputStream::write(bool value) {
  uint8_t temp = value;
  return write(&temp, 1);
}

int OutputStream::write(const std::string& str, bool widen) {
  return write_str(str.c_str(), str.length(), widen);
}

int OutputStream::write(const char* str, bool widen) {
  return write_str(str, strlen(str), widen);
}

int OutputStream::write_str(const char* str, uint32_t len, bool widen) {
  int ret = 0;
  if (!widen) {
    uint16_t shortLen = len;
    if (len != shortLen) {
      return -1;
    }
    ret = write(shortLen);
  } else {
    ret = write(len);
  }

  if (ret <= 0) {
    return ret;
  }

  if (len == 0) {
    return ret;
  }

  return ret + write(reinterpret_cast<const uint8_t *>(str), len);
}

int OutputStream::write(uint16_t value) {
  uint8_t buffer[2]{};

  buffer[0] = value >> 8;
  buffer[1] = value;

  return write(buffer, 2);
}

int OutputStream::write(uint32_t value) {
  uint8_t buffer[4]{};

  buffer[0] = value >> 24;
  buffer[1] = value >> 16;
  buffer[2] = value >> 8;
  buffer[3] = value;

  return write(buffer, 4);
}

int OutputStream::write(uint64_t value) {
  uint8_t buffer[8]{};

  buffer[0] = value >> 56;
  buffer[1] = value >> 48;
  buffer[2] = value >> 40;
  buffer[3] = value >> 32;
  buffer[4] = value >> 24;
  buffer[5] = value >> 16;
  buffer[6] = value >> 8;
  buffer[7] = value;

  return write(buffer, 8);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
