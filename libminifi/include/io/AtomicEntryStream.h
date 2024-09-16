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
#ifndef LIBMINIFI_INCLUDE_IO_ATOMICENTRYSTREAM_H_
#define LIBMINIFI_INCLUDE_IO_ATOMICENTRYSTREAM_H_

#include <memory>
#include <vector>
#include <mutex>
#include <cstring>
#include <algorithm>

#include "io/BaseStream.h"
#include "core/repository/AtomicRepoEntries.h"
#include "Exception.h"
#include "core/logging/LoggerFactory.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

template<typename T>
class AtomicEntryStream : public BaseStreamImpl {
 public:
  AtomicEntryStream(const T key, core::repository::AtomicEntry<T> *entry)
      : length_(0),
        offset_(0),
        key_(key),
        entry_(entry) {
    core::repository::RepoValue<T> *value;
    if (entry_->getValue(key, &value)) {
      length_ = value->getBuffer().size();
      entry_->decrementOwnership();
      invalid_stream_ = false;
    } else {
      invalid_stream_ = true;
    }
  }

  ~AtomicEntryStream() override;

  void close() override {
  }

  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(size_t offset) override;


  size_t tell() const override {
    return offset_;
  }


  size_t size() const override {
    return length_;
  }

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  size_t read(std::span<std::byte> buf) override;

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  size_t write(const uint8_t *value, size_t size) override;

 private:
  size_t length_;
  size_t offset_;
  T key_;
  core::repository::AtomicEntry<T> *entry_;
  std::atomic<bool> invalid_stream_;
  std::recursive_mutex entry_lock_;

  // Logger
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AtomicEntryStream>::getLogger();
};

template<typename T>
AtomicEntryStream<T>::~AtomicEntryStream() {
  logger_->log_debug("Decrementing");
  entry_->decrementOwnership();
}

template<typename T>
void AtomicEntryStream<T>::seek(size_t offset) {
  std::lock_guard<std::recursive_mutex> lock(entry_lock_);
  offset_ = gsl::narrow<size_t>(offset);
}


// data stream overrides
template<typename T>
size_t AtomicEntryStream<T>::write(const uint8_t *value, size_t size) {
  if (size == 0) return 0;
  if (!value || invalid_stream_) return STREAM_ERROR;
  const auto out_span = gsl::make_span(value, size).as_span<const std::byte>();
  std::lock_guard<std::recursive_mutex> lock(entry_lock_);
  if (entry_->insert(key_, out_span)) {
    offset_ += size;
    if (offset_ > length_) {
      length_ = offset_;
    }
    return size;
  }
  return STREAM_ERROR;
}

template<typename T>
size_t AtomicEntryStream<T>::read(std::span<std::byte> buf) {
  if (buf.empty()) {
    return 0;
  }
  if (!invalid_stream_) {
    std::lock_guard<std::recursive_mutex> lock(entry_lock_);
    auto len = buf.size();
    core::repository::RepoValue<T> *value;
    if (entry_->getValue(key_, &value)) {
      if (offset_ + len > value->getBuffer().size()) {
        len = value->getBuffer().size() - offset_;
        if (len <= 0) {
          entry_->decrementOwnership();
          return 0;
        }
      }
      const auto src_buffer = value->getBuffer().subspan(offset_, len);
      std::memcpy(buf.data(), src_buffer.data(), src_buffer.size());
      offset_ += len;
      entry_->decrementOwnership();
      return len;
    }
  }
  return STREAM_ERROR;
}

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_IO_ATOMICENTRYSTREAM_H_
