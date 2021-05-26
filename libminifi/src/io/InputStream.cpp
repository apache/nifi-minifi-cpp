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
#include <vector>
#include <string>
#include "io/InputStream.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

size_t InputStream::read(std::vector<uint8_t>& buffer, size_t len) {
  if (buffer.size() < len) {
    buffer.resize(len);
  }
  const auto ret = read(buffer.data(), len);
  if (io::isError(ret)) return ret;
  buffer.resize(ret);
  return ret;
}

size_t InputStream::read(bool &value) {
  uint8_t buf = 0;

  if (read(&buf, 1) != 1) {
    return STREAM_ERROR;
  }
  value = buf;
  return 1;
}

size_t InputStream::read(utils::Identifier &value) {
  std::string uuidStr;
  const auto ret = read(uuidStr);
  if (isError(ret)) {
    return ret;
  }
  auto optional_uuid = utils::Identifier::parse(uuidStr);
  if (!optional_uuid) {
    return STREAM_ERROR;
  }
  value = optional_uuid.value();
  return ret;
}

size_t InputStream::read(std::string &str, bool widen) {
  uint32_t string_length = 0;
  size_t length_return = 0;
  if (!widen) {
    uint16_t shortLength = 0;
    length_return = read(shortLength);
    string_length = shortLength;
  } else {
    length_return = read(string_length);
  }

  if (length_return == 0 || isError(length_return)) {
    return length_return;
  }

  if (string_length == 0) {
    str.clear();
    return length_return;
  }

  std::vector<uint8_t> buffer(string_length);
  const auto read_return = read(buffer.data(), string_length);
  if (read_return != string_length) {
    return read_return;
  }

  str = std::string(reinterpret_cast<const char*>(buffer.data()), string_length);
  return length_return + string_length;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
