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
#ifdef WIN32
#include "Winsock2.h"
#else
#include <arpa/inet.h>
#endif
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

#define htonll_r(x) ((((uint64_t)htonl(x)) << 32) + htonl((x) >> 32))
#define IS_ASCII(c) __builtin_expect(!!((c >= 1) && (c <= 127)), 1)

template<typename T>
int Serializable::writeData(const T &t, DataStream *stream) {
  uint8_t bytes[sizeof t];
  std::copy(static_cast<const char*>(static_cast<const void*>(&t)), static_cast<const char*>(static_cast<const void*>(&t)) + sizeof t, bytes);
  return stream->writeData(bytes, sizeof t);
}

template<typename T>
int Serializable::writeData(const T &t, uint8_t *to_vec) {
  std::copy(static_cast<const char*>(static_cast<const void*>(&t)), static_cast<const char*>(static_cast<const void*>(&t)) + sizeof t, to_vec);
  return sizeof t;
}

template<typename T>
int Serializable::writeData(const T &t, std::vector<uint8_t> &to_vec) {
  uint8_t bytes[sizeof t];
  std::copy(static_cast<const char*>(static_cast<const void*>(&t)), static_cast<const char*>(static_cast<const void*>(&t)) + sizeof t, bytes);
  to_vec.insert(to_vec.end(), &bytes[0], &bytes[sizeof t]);
  return sizeof t;
}

int Serializable::write(uint8_t value, DataStream *stream) {
  return stream->writeData(&value, 1);
}
int Serializable::write(char value, DataStream *stream) {
  return stream->writeData(reinterpret_cast<uint8_t *>(&value), 1);
}

int Serializable::write(uint8_t *value, int len, DataStream *stream) {
  return stream->writeData(value, len);
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

int Serializable::read(uint16_t &value, DataStream *stream, bool is_little_endian) {
  return stream->read(value, is_little_endian);
}

int Serializable::read(uint32_t &value, DataStream *stream, bool is_little_endian) {
  return stream->read(value, is_little_endian);
}
int Serializable::read(uint64_t &value, DataStream *stream, bool is_little_endian) {
  return stream->read(value, is_little_endian);
}

int Serializable::write(uint32_t base_value, DataStream *stream, bool is_little_endian) {
  const uint32_t value = is_little_endian ? htonl(base_value) : base_value;

  return writeData(value, stream);
}

int Serializable::write(uint64_t base_value, DataStream *stream, bool is_little_endian) {
  const uint64_t value = is_little_endian == 1 ? htonll_r(base_value) : base_value;
  return writeData(value, stream);
}

int Serializable::write(uint16_t base_value, DataStream *stream, bool is_little_endian) {
  const uint16_t value = is_little_endian == 1 ? htons(base_value) : base_value;

  return writeData(value, stream);
}

int Serializable::readUTF(std::string &str, DataStream *stream, bool widen) {
  uint32_t utflen = 0;
  int ret = 1;
  if (!widen) {
    uint16_t shortLength = 0;
    ret = read(shortLength, stream);
    utflen = shortLength;
    if (ret <= 0)
      return ret;
  } else {
    uint32_t len;
    ret = read(len, stream);
    if (ret <= 0)
      return ret;
    utflen = len;
  }

  if (utflen == 0) {
    str = "";
    return 1;
  }

  std::vector<uint8_t> buf;
  ret = stream->readData(buf, utflen);

  // The number of chars produced may be less than utflen
  str = std::string((const char*) &buf[0], utflen);
  return utflen;
}

int Serializable::writeUTF(std::string str, DataStream *stream, bool widen) {
  uint32_t utflen = 0;

  utflen = str.length();

  if (utflen > 65535)
    return -1;

  if (utflen == 0) {
    if (!widen) {
      uint16_t shortLen = utflen;
      write(shortLen, stream);
    } else {
      write(utflen, stream);
    }
    return 1;
  }

  std::vector<uint8_t> utf_to_write;
  if (!widen) {
    utf_to_write.resize(utflen);
  } else {
    utf_to_write.resize(utflen);
  }

  uint8_t *underlyingPtr = &utf_to_write[0];
  for (auto c : str) {
    writeData(c, underlyingPtr++);
  }
  int ret;

  if (!widen) {
    uint16_t short_length = utflen;
    write(short_length, stream);
    ret = stream->writeData(utf_to_write.data(), utflen);
  } else {
    write(utflen, stream);
    ret = stream->writeData(utf_to_write.data(), utflen);
  }
  return ret;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
