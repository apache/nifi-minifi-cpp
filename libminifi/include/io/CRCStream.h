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
#ifndef LIBMINIFI_INCLUDE_IO_CRCSTREAM_H_
#define LIBMINIFI_INCLUDE_IO_CRCSTREAM_H_

#include <zlib.h>
#include <memory>
#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "BaseStream.h"
#include "Serializable.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

#define htonll_r(x) ((((uint64_t)htonl(x)) << 32) + htonl((x) >> 32))

template<typename T>
class CRCStream : public BaseStream {
 public:
  /**
   * Raw pointer because the caller guarantees that
   * it will exceed our lifetime.
   */
  explicit CRCStream(T *child_stream);
  explicit CRCStream(T *child_stream, uint64_t initial_crc);

  CRCStream(CRCStream<T>&&) noexcept;

  ~CRCStream() override = default;

  T *getstream() const {
    return child_stream_;
  }

  void disableEncoding() {
    disable_encoding_ = true;
  }

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  int readData(std::vector<uint8_t> &buf, int buflen) override;
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  int readData(uint8_t *buf, int buflen) override;

  /**
   * Write value to the stream using std::vector
   * @param buf incoming buffer
   * @param buflen buffer to write
   */
  virtual int writeData(std::vector<uint8_t> &buf, int buflen);

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  int writeData(uint8_t *value, int size) override;

  /**
   * write 4 bytes to stream
   * @param base_value non encoded value
   * @param stream output stream
   * @param is_little_endian endianness determination
   * @return resulting write size
   **/
  int write(uint32_t base_value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;
  /**
   * write 2 bytes to stream
   * @param base_value non encoded value
   * @param stream output stream
   * @param is_little_endian endianness determination
   * @return resulting write size
   **/
  int write(uint16_t base_value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;

  /**
   * write 8 bytes to stream
   * @param base_value non encoded value
   * @param stream output stream
   * @param is_little_endian endianness determination
   * @return resulting write size
   **/
  int write(uint64_t base_value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;

  /**
   * Reads a system word
   * @param value value to write
   */
  int read(uint64_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;

  /**
   * Reads a uint32_t
   * @param value value to write
   */
  int read(uint32_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;

  /**
   * Reads a system short
   * @param value value to write
   */
  int read(uint16_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;

  const uint64_t getSize() const override { return child_stream_->getSize(); }

  void closeStream() override { child_stream_->closeStream(); }

  short initialize() override {
    child_stream_->initialize();
    reset();
    return 0;
  }

  void updateCRC(uint8_t *buffer, uint32_t length);

  uint64_t getCRC() {
    return crc_;
  }

  void reset();
 protected:

  /**
   * Creates a vector and returns the vector using the provided
   * type name.
   * @param t incoming object
   * @returns vector.
   */
  template<typename K>
  std::vector<uint8_t> readBuffer(const K& t) {
    std::vector<uint8_t> buf;
    readBuffer(buf, t);
    return buf;
  }

  /**
   * Populates the vector using the provided type name.
   * @param buf output buffer
   * @param t incoming object
   * @returns number of bytes read.
   */
  template<typename K>
  int readBuffer(std::vector<uint8_t>& buf, const K& t) {
    buf.resize(sizeof t);
    return readData((uint8_t*) &buf[0], sizeof(t));
  }

  uint64_t crc_;
  T *child_stream_;
  bool disable_encoding_;
};

template<typename T>
CRCStream<T>::CRCStream(T *child_stream)
    : child_stream_(child_stream),
      disable_encoding_(false) {
  crc_ = crc32(0L, Z_NULL, 0);
}

template<typename T>
CRCStream<T>::CRCStream(T *child_stream, uint64_t initial_crc)
    : crc_(initial_crc),
      child_stream_(child_stream),
      disable_encoding_(false) {
}

template<typename T>
CRCStream<T>::CRCStream(CRCStream<T> &&move) noexcept
    : crc_(std::move(move.crc_)),
      child_stream_(std::move(move.child_stream_)),
      disable_encoding_(false) {
}

template<typename T>
int CRCStream<T>::readData(std::vector<uint8_t> &buf, int buflen) {
  if (buflen < 0) {
    throw minifi::Exception{ExceptionType::GENERAL_EXCEPTION, "negative buflen"};
  }

  if (buf.size() < static_cast<size_t>(buflen))
    buf.resize(buflen);
  return readData(buf.data(), buflen);
}

template<typename T>
int CRCStream<T>::readData(uint8_t *buf, int buflen) {
  int ret = child_stream_->read(buf, buflen);
  if (ret > 0) {
    crc_ = crc32(crc_, buf, ret);
  }
  return ret;
}

template<typename T>
int CRCStream<T>::writeData(std::vector<uint8_t> &buf, int buflen) {
  if (buflen < 0) {
    throw minifi::Exception{ExceptionType::GENERAL_EXCEPTION, "negative buflen"};
  }

  if (buf.size() < static_cast<size_t>(buflen))
    buf.resize(buflen);
  return writeData(buf.data(), buflen);
}

template<typename T>
int CRCStream<T>::writeData(uint8_t *value, int size) {
  int ret = child_stream_->write(value, size);
  if (ret > 0) {
    crc_ = crc32(crc_, value, ret);
  }
  return ret;
}
template<typename T>
void CRCStream<T>::reset() {
  crc_ = crc32(0L, Z_NULL, 0);
}
template<typename T>
void CRCStream<T>::updateCRC(uint8_t *buffer, uint32_t length) {
  crc_ = crc32(crc_, buffer, length);
}

template<typename T>
int CRCStream<T>::write(uint64_t base_value, bool is_little_endian) {
  if (disable_encoding_)
    is_little_endian = false;
  const uint64_t value = is_little_endian == 1 ? htonll_r(base_value) : base_value;
  uint8_t bytes[sizeof value];
  std::copy(static_cast<const char*>(static_cast<const void*>(&value)), static_cast<const char*>(static_cast<const void*>(&value)) + sizeof value, bytes);
  return writeData(bytes, sizeof value);
}

template<typename T>
int CRCStream<T>::write(uint32_t base_value, bool is_little_endian) {
  if (disable_encoding_)
    is_little_endian = false;
  const uint32_t value = is_little_endian ? htonl(base_value) : base_value;
  uint8_t bytes[sizeof value];
  std::copy(static_cast<const char*>(static_cast<const void*>(&value)), static_cast<const char*>(static_cast<const void*>(&value)) + sizeof value, bytes);
  return writeData(bytes, sizeof value);
}

template<typename T>
int CRCStream<T>::write(uint16_t base_value, bool is_little_endian) {
  if (disable_encoding_)
    is_little_endian = false;
  const uint16_t value = is_little_endian == 1 ? htons(base_value) : base_value;
  uint8_t bytes[sizeof value];
  std::copy(static_cast<const char*>(static_cast<const void*>(&value)), static_cast<const char*>(static_cast<const void*>(&value)) + sizeof value, bytes);
  return writeData(bytes, sizeof value);
}

template<typename T>
int CRCStream<T>::read(uint64_t &value, bool is_little_endian) {
  if (disable_encoding_)
    is_little_endian = false;
  std::vector<uint8_t> buf;
  auto ret = readBuffer(buf, value);
  if(ret <= 0)return ret;

  if (is_little_endian) {
    value = ((uint64_t) buf[0] << 56) | ((uint64_t) (buf[1] & 255) << 48) | ((uint64_t) (buf[2] & 255) << 40) | ((uint64_t) (buf[3] & 255) << 32) | ((uint64_t) (buf[4] & 255) << 24)
        | ((uint64_t) (buf[5] & 255) << 16) | ((uint64_t) (buf[6] & 255) << 8) | ((uint64_t) (buf[7] & 255) << 0);
  } else {
    value = ((uint64_t) buf[0] << 0) | ((uint64_t) (buf[1] & 255) << 8) | ((uint64_t) (buf[2] & 255) << 16) | ((uint64_t) (buf[3] & 255) << 24) | ((uint64_t) (buf[4] & 255) << 32)
        | ((uint64_t) (buf[5] & 255) << 40) | ((uint64_t) (buf[6] & 255) << 48) | ((uint64_t) (buf[7] & 255) << 56);
  }
  return sizeof(value);
}

template<typename T>
int CRCStream<T>::read(uint32_t &value, bool is_little_endian) {
  if (disable_encoding_)
    is_little_endian = false;
  std::vector<uint8_t> buf;
  auto ret = readBuffer(buf, value);
  if(ret <= 0)return ret;

  if (is_little_endian) {
    value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  } else {
    value = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
  }

  return sizeof(value);
}

template<typename T>
int CRCStream<T>::read(uint16_t &value, bool is_little_endian) {
  if (disable_encoding_)
    is_little_endian = false;
  std::vector<uint8_t> buf;
  auto ret = readBuffer(buf, value);
  if(ret <= 0)return ret;

  if (is_little_endian) {
    value = (buf[0] << 8) | buf[1];
  } else {
    value = buf[0] | buf[1] << 8;
  }
  return sizeof(value);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_IO_CRCSTREAM_H_ */
