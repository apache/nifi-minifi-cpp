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

#include "io/DescriptorStream.h"
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include "io/validation.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

DescriptorStream::DescriptorStream(int fd)
    : fd_(fd),
      logger_(logging::LoggerFactory<DescriptorStream>::getLogger()) {
}

void DescriptorStream::seek(uint64_t offset) {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
  lseek(fd_, offset, 0x00);
}

int DescriptorStream::writeData(std::vector<uint8_t> &buf, int buflen) {
  if (static_cast<int>(buf.capacity()) < buflen) {
    return -1;
  }
  return writeData(reinterpret_cast<uint8_t *>(&buf[0]), buflen);
}

// data stream overrides

int DescriptorStream::writeData(uint8_t *value, int size) {
  if (!IsNullOrEmpty(value)) {
    std::lock_guard<std::recursive_mutex> lock(file_lock_);
    if (::write(fd_, value, size) != size) {
      return -1;
    } else {
      return size;
    }
  } else {
    return -1;
  }
}

template<typename T>
inline std::vector<uint8_t> DescriptorStream::readBuffer(const T& t) {
  std::vector<uint8_t> buf;
  buf.resize(sizeof t);
  readData(reinterpret_cast<uint8_t *>(&buf[0]), sizeof(t));
  return buf;
}

int DescriptorStream::readData(std::vector<uint8_t> &buf, int buflen) {
  if (static_cast<int>(buf.capacity()) < buflen) {
    buf.resize(buflen);
  }
  int ret = readData(reinterpret_cast<uint8_t*>(&buf[0]), buflen);

  if (ret < buflen) {
    buf.resize(ret);
  }
  return ret;
}

int DescriptorStream::readData(uint8_t *buf, int buflen) {
  if (!IsNullOrEmpty(buf)) {
    auto size_read = ::read(fd_, buf, buflen);

    if (size_read != buflen) {
      return -1;
    } else {
      return buflen;
    }

  } else {
    return -1;
  }
}

/**
 * reads a byte from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int DescriptorStream::read(uint8_t &value) {
  return Serializable::read(value, reinterpret_cast<DataStream*>(this));
}

/**
 * reads two bytes from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int DescriptorStream::read(uint16_t &base_value, bool is_little_endian) {
  auto buf = readBuffer(base_value);
  if (is_little_endian) {
    base_value = (buf[0] << 8) | buf[1];
  } else {
    base_value = buf[0] | buf[1] << 8;
  }
  return 2;
}

/**
 * reads a byte from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int DescriptorStream::read(char &value) {
  return readData(reinterpret_cast<uint8_t*>(&value), 1);
}

/**
 * reads a byte array from the stream
 * @param value reference in which will set the result
 * @param len length to read
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int DescriptorStream::read(uint8_t *value, int len) {
  return readData(value, len);
}

/**
 * reads four bytes from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int DescriptorStream::read(uint32_t &value, bool is_little_endian) {
  auto buf = readBuffer(value);
  if (is_little_endian) {
    value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  } else {
    value = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
  }
  return 4;
}

/**
 * reads eight byte from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int DescriptorStream::read(uint64_t &value, bool is_little_endian) {
  auto buf = readBuffer(value);

  if (is_little_endian) {
    value = ((uint64_t) buf[0] << 56) | ((uint64_t) (buf[1] & 255) << 48) | ((uint64_t) (buf[2] & 255) << 40) | ((uint64_t) (buf[3] & 255) << 32) | ((uint64_t) (buf[4] & 255) << 24)
        | ((uint64_t) (buf[5] & 255) << 16) | ((uint64_t) (buf[6] & 255) << 8) | ((uint64_t) (buf[7] & 255) << 0);
  } else {
    value = ((uint64_t) buf[0] << 0) | ((uint64_t) (buf[1] & 255) << 8) | ((uint64_t) (buf[2] & 255) << 16) | ((uint64_t) (buf[3] & 255) << 24) | ((uint64_t) (buf[4] & 255) << 32)
        | ((uint64_t) (buf[5] & 255) << 40) | ((uint64_t) (buf[6] & 255) << 48) | ((uint64_t) (buf[7] & 255) << 56);
  }
  return 8;
}

/**
 * read UTF from stream
 * @param str reference string
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int DescriptorStream::readUTF(std::string &str, bool widen) {
  return Serializable::readUTF(str, reinterpret_cast<DataStream*>(this), widen);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

