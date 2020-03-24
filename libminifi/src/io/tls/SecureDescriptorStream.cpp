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

#include "io/tls/SecureDescriptorStream.h"
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <Exception.h>
#include "io/validation.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

SecureDescriptorStream::SecureDescriptorStream(int fd, SSL *ssl)
    : fd_(fd), ssl_(ssl),
      logger_(logging::LoggerFactory<SecureDescriptorStream>::getLogger()) {
}

void SecureDescriptorStream::seek(uint64_t offset) {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
  lseek(fd_, offset, 0x00);
}

int SecureDescriptorStream::writeData(std::vector<uint8_t> &buf, int buflen) {
  if (buflen < 0) {
    throw minifi::Exception{ExceptionType::GENERAL_EXCEPTION, "negative buflen"};
  }

  if (buf.size() < static_cast<size_t>(buflen)) {
    return -1;
  }
  return writeData(buf.data(), buflen);
}

// data stream overrides

int SecureDescriptorStream::writeData(uint8_t *value, int size) {
  if (!IsNullOrEmpty(value)) {
    std::lock_guard<std::recursive_mutex> lock(file_lock_);
    int bytes = 0;
     int sent = 0;
     while (bytes < size) {
       sent = SSL_write(ssl_, value + bytes, size - bytes);
       // check for errors
       if (sent < 0) {
         int ret = 0;
         ret = SSL_get_error(ssl_, sent);
         logger_->log_error("WriteData socket %d send failed %s %d", fd_, strerror(errno), ret);
         return sent;
       }
       bytes += sent;
     }
     return size;
  } else {
    return -1;
  }
}

template<typename T>
inline std::vector<uint8_t> SecureDescriptorStream::readBuffer(const T& t) {
  std::vector<uint8_t> buf;
  buf.resize(sizeof t);
  readData(reinterpret_cast<uint8_t *>(&buf[0]), sizeof(t));
  return buf;
}

int SecureDescriptorStream::readData(std::vector<uint8_t> &buf, int buflen) {
  if (buflen < 0) {
    throw minifi::Exception{ExceptionType::GENERAL_EXCEPTION, "negative buflen"};
  }

  if (buf.size() < static_cast<size_t>(buflen)) {
    buf.resize(buflen);
  }
  int ret = readData(buf.data(), buflen);

  if (ret < buflen) {
    buf.resize((std::max)(ret, 0));
  }
  return ret;
}

int SecureDescriptorStream::readData(uint8_t *buf, int buflen) {
  if (!IsNullOrEmpty(buf)) {
    int total_read = 0;
      int status = 0;
      while (buflen) {
        int sslStatus;
        do {
          status = SSL_read(ssl_, buf, buflen);
          sslStatus = SSL_get_error(ssl_, status);
        } while (status < 0 && sslStatus == SSL_ERROR_WANT_READ);

        if (status < 0)
              break;

        buflen -= status;
        buf += status;
        total_read += status;
      }

      return total_read;

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
int SecureDescriptorStream::read(uint8_t &value) {
  return Serializable::read(value, reinterpret_cast<DataStream*>(this));
}

/**
 * reads two bytes from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int SecureDescriptorStream::read(uint16_t &base_value, bool is_little_endian) {
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
int SecureDescriptorStream::read(char &value) {
  return readData(reinterpret_cast<uint8_t*>(&value), 1);
}

/**
 * reads a byte array from the stream
 * @param value reference in which will set the result
 * @param len length to read
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int SecureDescriptorStream::read(uint8_t *value, int len) {
  return readData(value, len);
}

/**
 * reads four bytes from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int SecureDescriptorStream::read(uint32_t &value, bool is_little_endian) {
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
int SecureDescriptorStream::read(uint64_t &value, bool is_little_endian) {
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
int SecureDescriptorStream::readUTF(std::string &str, bool widen) {
  return Serializable::readUTF(str, reinterpret_cast<DataStream*>(this), widen);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

