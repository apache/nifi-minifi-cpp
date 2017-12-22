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

#include <mutex>
#include <cstring>
#include "BaseStream.h"
#include "core/repository/AtomicRepoEntries.h"
#include "Exception.h"
#include "core/logging/LoggerConfiguration.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

template<typename T>
class AtomicEntryStream : public BaseStream {
 public:
  AtomicEntryStream(const T key, core::repository::AtomicEntry<T> *entry)
      : key_(key),
        entry_(entry),
        offset_(0),
        length_(0),
        logger_(logging::LoggerFactory<AtomicEntryStream()>::getLogger()) {
    core::repository::RepoValue<T> *value;
    if (entry_->getValue(key, &value)) {
      length_ = value->getBufferSize();
      entry_->decrementOwnership();
      invalid_stream_ = false;
    } else {
      invalid_stream_ = true;
    }
  }

  virtual ~AtomicEntryStream();

  virtual void closeStream() {

  }

  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(uint64_t offset);

  virtual const uint64_t getSize() const {
    return length_;
  }

  // data stream extensions
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(std::vector<uint8_t> &buf, int buflen);
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(uint8_t *buf, int buflen);

  /**
   * Write value to the stream using std::vector
   * @param buf incoming buffer
   * @param buflen buffer to write
   *
   */
  virtual int writeData(std::vector<uint8_t> &buf, int buflen);

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  virtual int writeData(uint8_t *value, int size);

  /**
   * Returns the underlying buffer
   * @return vector's array
   **/
  const uint8_t *getBuffer() const {
    throw std::runtime_error("Stream does not support this operation");
  }

 protected:
  size_t length_;
  size_t offset_;
  T key_;
  core::repository::AtomicEntry<T> *entry_;
  std::atomic<bool> invalid_stream_;
  std::recursive_mutex entry_lock_;

  // Logger
  std::shared_ptr<logging::Logger> logger_;

};

template<typename T>
AtomicEntryStream<T>::~AtomicEntryStream() {
  logger_->log_debug("Decrementing");
  entry_->decrementOwnership();
}

template<typename T>
void AtomicEntryStream<T>::seek(uint64_t offset) {
  std::lock_guard<std::recursive_mutex> lock(entry_lock_);
  offset_ = offset;
}

template<typename T>
int AtomicEntryStream<T>::writeData(std::vector<uint8_t> &buf, int buflen) {
  if ((int)buf.capacity() < buflen || invalid_stream_)
    return -1;
  return writeData(reinterpret_cast<uint8_t *>(&buf[0]), buflen);
}

// data stream overrides
template<typename T>
int AtomicEntryStream<T>::writeData(uint8_t *value, int size) {
  if (nullptr != value && !invalid_stream_) {
    std::lock_guard<std::recursive_mutex> lock(entry_lock_);
    if (entry_->insert(key_, value, size)) {
      offset_ += size;
      if (offset_ > length_) {
        length_ = offset_;
      }
      return size;
    }

  }
  return -1;

}

template<typename T>
int AtomicEntryStream<T>::readData(std::vector<uint8_t> &buf, int buflen) {
  if (invalid_stream_) {
    return -1;
  }
  if ((int)buf.capacity() < buflen) {
    buf.resize(buflen);
  }
  int ret = readData(reinterpret_cast<uint8_t*>(&buf[0]), buflen);

  if (ret < buflen) {
    buf.resize(ret);
  }
  return ret;
}

template<typename T>
int AtomicEntryStream<T>::readData(uint8_t *buf, int buflen) {
  if (nullptr != buf && !invalid_stream_) {
    std::lock_guard<std::recursive_mutex> lock(entry_lock_);
    int len = buflen;
    core::repository::RepoValue<T> *value;
    if (entry_->getValue(key_, &value)) {
      if (offset_ + len > value->getBufferSize()) {
        len = value->getBufferSize() - offset_;
        if (len <= 0) {
          entry_->decrementOwnership();
          return 0;
        }
      }
      std::memcpy(buf, reinterpret_cast<uint8_t*>(const_cast<uint8_t*>(value->getBuffer()) + offset_), len);
      offset_ += len;
      entry_->decrementOwnership();
      return len;
    }

  }
  return -1;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_ATOMICENTRYSTREAM_H_ */
