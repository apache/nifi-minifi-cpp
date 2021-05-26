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
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "io/OutputStream.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

size_t OutputStream::write(const std::vector<uint8_t>& buffer, size_t len) {
  if (buffer.size() < len) {
    return STREAM_ERROR;
  }
  return write(buffer.data(), len);
}

size_t OutputStream::write(bool value) {
  uint8_t temp = value;
  return write(&temp, 1);
}

size_t OutputStream::write(const utils::Identifier &value) {
  return write(value.to_string());
}

size_t OutputStream::write(const std::string& str, bool widen) {
  return write_str(str.c_str(), gsl::narrow<uint32_t>(str.length()), widen);
}

size_t OutputStream::write(const char* str, bool widen) {
  return write_str(str, gsl::narrow<uint32_t>(std::strlen(str)), widen);
}

size_t OutputStream::write_str(const char* str, uint32_t len, bool widen) {
  size_t ret = 0;
  if (!widen) {
    const auto shortLen = gsl::narrow_cast<uint16_t>(len);
    if (len != shortLen) {
      return STREAM_ERROR;
    }
    ret = write(shortLen);
  } else {
    ret = write(len);
  }

  if (ret == 0 || isError(ret)) {
    return ret;
  }

  if (len == 0) {
    return ret;
  }

  return ret + write(reinterpret_cast<const uint8_t *>(str), len);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
