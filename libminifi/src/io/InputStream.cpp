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

int InputStream::read(std::vector<uint8_t>& buffer, int len) {
  if (buffer.size() < len) {
    buffer.resize(len);
  }
  int ret = read(buffer.data(), len);
  buffer.resize((std::max)(ret, 0));
  return ret;
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

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
