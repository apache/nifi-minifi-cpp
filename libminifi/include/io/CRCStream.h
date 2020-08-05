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

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#ifdef WIN32
#include <winsock2.h>

#else
#include <arpa/inet.h>

#endif
#include "BaseStream.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

template<typename T>
class CRCStream : public BaseStream {
 public:
  /**
   * Raw pointer because the caller guarantees that
   * it will exceed our lifetime.
   */
  explicit CRCStream(T *child_stream);
  CRCStream(T *child_stream, uint64_t initial_crc);

  CRCStream(CRCStream<T>&&) noexcept;

  ~CRCStream() override = default;

  T *getstream() const {
    return child_stream_;
  }

  using BaseStream::write;
  using BaseStream::read;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  int read(uint8_t *buf, unsigned int buflen) override;

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  int write(const uint8_t *value, unsigned int size) override;

  size_t size() const override { return child_stream_->size(); }

  void close() override { child_stream_->close(); }

  int initialize() override {
    child_stream_->initialize();
    reset();
    return 0;
  }

  void updateCRC(uint8_t *buffer, uint32_t length);

  uint64_t getCRC() {
    return crc_;
  }

  void reset();

 private:
  uLong crc_;
  T *child_stream_;
};

template<typename T>
CRCStream<T>::CRCStream(T *child_stream)
    : child_stream_(child_stream) {
  crc_ = crc32(0L, Z_NULL, 0);
}

template<typename T>
CRCStream<T>::CRCStream(T *child_stream, uint64_t initial_crc)
    : crc_(gsl::narrow<uLong>(initial_crc)),
      child_stream_(child_stream) {
}

template<typename T>
CRCStream<T>::CRCStream(CRCStream<T> &&move) noexcept
    : crc_(std::move(move.crc_)),
      child_stream_(std::move(move.child_stream_)) {
}

template<typename T>
int CRCStream<T>::read(uint8_t *buf, unsigned int buflen) {
  int ret = child_stream_->read(buf, buflen);
  if (ret > 0) {
    crc_ = crc32(crc_, buf, ret);
  }
  return ret;
}

template<typename T>
int CRCStream<T>::write(const uint8_t *value, unsigned int size) {
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

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_IO_CRCSTREAM_H_
