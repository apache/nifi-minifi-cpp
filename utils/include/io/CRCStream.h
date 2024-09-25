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
#include <cassert>

#ifdef WIN32
// ignore the warning about inheriting via dominance from CRCStreamBase
#pragma warning(disable : 4250)
#include <winsock2.h>

#else
#include <arpa/inet.h>

#endif
#include "InputStream.h"
#include "OutputStream.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {
namespace internal {

template<typename StreamType>
class CRCStreamBase : public virtual StreamImpl {
 public:
  StreamType *getstream() const {
    return child_stream_;
  }

  void close() override { child_stream_->close(); }

  int initialize() override {
    child_stream_->initialize();
    reset();
    return 0;
  }

  void updateCRC(uint8_t *buffer, uint32_t length) {
    crc_ = crc32(crc_, buffer, length);
  }

  uint64_t getCRC() {
    return gsl::narrow<uint64_t>(crc_);
  }

  void reset() {
    crc_ = crc32(0L, Z_NULL, 0);
  }

 protected:
  uLong crc_ = 0;
  StreamType* child_stream_ = nullptr;
};

template<typename StreamType>
class InputCRCStream : public virtual CRCStreamBase<StreamType>, public virtual InputStream {
 protected:
  using CRCStreamBase<StreamType>::child_stream_;
  using CRCStreamBase<StreamType>::crc_;

 public:
  using InputStream::read;

  size_t read(std::span<std::byte> buf) override {
    const auto ret = child_stream_->read(buf);
    if (ret > 0 && !io::isError(ret)) {
      crc_ = crc32(crc_, reinterpret_cast<const unsigned char*>(buf.data()), gsl::narrow<uInt>(ret));
    }
    return ret;
  }

  size_t size() const override { return child_stream_->size(); }
};

template<typename StreamType>
class OutputCRCStream : public virtual CRCStreamBase<StreamType>, public virtual OutputStream {
 protected:
  using CRCStreamBase<StreamType>::child_stream_;
  using CRCStreamBase<StreamType>::crc_;

 public:
  using OutputStream::write;

  size_t write(const uint8_t *value, size_t size) override {
    const auto ret = child_stream_->write(value, size);
    if (ret > 0 && !io::isError(ret)) {
      crc_ = crc32(crc_, value, gsl::narrow<uInt>(ret));
    }
    return ret;
  }
};

struct empty_class {};

}  // namespace internal

template<typename StreamType>
class CRCStream : public std::conditional<std::is_base_of<InputStream, StreamType>::value, internal::InputCRCStream<StreamType>, internal::empty_class>::type
                  , public std::conditional<std::is_base_of<OutputStream, StreamType>::value, internal::OutputCRCStream<StreamType>, internal::empty_class>::type {
  using internal::CRCStreamBase<StreamType>::child_stream_;
  using internal::CRCStreamBase<StreamType>::crc_;

 public:
  explicit CRCStream(gsl::not_null<StreamType*> child_stream) {
    child_stream_ = child_stream;
    crc_ = crc32(0L, Z_NULL, 0);
  }

  CRCStream(gsl::not_null<StreamType*> child_stream, uint64_t initial_crc) {
    child_stream_ = child_stream;
    crc_ = gsl::narrow<uLong>(initial_crc);
  }

  CRCStream(CRCStream &&stream) noexcept {
    child_stream_ = std::exchange(stream.child_stream_, nullptr);
    crc_ = std::exchange(stream.crc_, 0);
  }
};


}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_IO_CRCSTREAM_H_
