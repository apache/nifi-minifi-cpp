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
#include "io/Serializable.h"
#include <cstdio>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "io/DataStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

#define IS_ASCII(c) __builtin_expect(!!((c >= 1) && (c <= 127)), 1)

int Serializable::write(uint8_t value, DataStream *stream) {
  return stream->writeData(&value, 1);
}
int Serializable::write(char value, DataStream *stream) {
  return stream->writeData(reinterpret_cast<uint8_t *>(&value), 1);
}

int Serializable::write(const uint8_t * const value, int len, DataStream *stream) {
  return stream->writeData(const_cast<uint8_t *>(value), len);
}

int Serializable::write(bool value, DataStream *stream) {
  uint8_t temp = value;
  return stream->writeData(&temp, 1);
}

int Serializable::read(uint8_t &value, DataStream *stream) {
  uint8_t buf;

  int ret = stream->readData(&buf, 1);
  if (ret == 1)
    value = buf;
  return ret;
}

int Serializable::read(char &value, DataStream *stream) {
  uint8_t buf;

  int ret = stream->readData(&buf, 1);
  if (ret == 1)
    value = buf;
  return ret;
}

int Serializable::read(uint8_t *value, int len, DataStream *stream) {
  return stream->readData(value, len);
}

int Serializable::readUTF(std::string &str, DataStream *stream, bool widen) {
  uint32_t utflen = 0;
  int ret = 1;
  if (!widen) {
    uint16_t shortLength = 0;
    ret = read(shortLength, stream);
    utflen = shortLength;
  } else {
    ret = read(utflen, stream);
  }

  if (ret <= 0)
    return ret;

  if (utflen == 0) {
    str = "";
    return 1;
  }

  std::vector<uint8_t> buf;
  stream->readData(buf, utflen);

  // The number of chars produced may be less than utflen
  str = std::string((const char*) &buf[0], utflen);
  return utflen;
}

int Serializable::writeUTF(std::string str, DataStream *stream, bool widen) {
  uint32_t utflen = 0;

  utflen = str.length();

  if (utflen > 65535)
    return -1;

  if (!widen) {
    uint16_t shortLen = utflen;
    write(shortLen, stream);
  } else {
    write(utflen, stream);
  }

  if (utflen == 0) {
    return 1;
  }

  return stream->writeData(reinterpret_cast<uint8_t *>(const_cast<char*>(str.c_str())), utflen);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
