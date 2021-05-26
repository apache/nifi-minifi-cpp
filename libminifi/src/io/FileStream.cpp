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

#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <Exception.h>
#include "io/validation.h"
#include "io/FileStream.h"
#include "io/InputStream.h"
#include "io/OutputStream.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

constexpr const char *FILE_OPENING_ERROR_MSG = "Error opening file: ";
constexpr const char *READ_ERROR_MSG = "Error reading from file: ";
constexpr const char *WRITE_ERROR_MSG = "Error writing to file: ";
constexpr const char *SEEK_ERROR_MSG = "Error seeking in file: ";
constexpr const char *INVALID_FILE_STREAM_ERROR_MSG = "invalid file stream";
constexpr const char *TELLG_CALL_ERROR_MSG = "tellg call on file stream failed";
constexpr const char *INVALID_BUFFER_ERROR_MSG = "invalid buffer";
constexpr const char *FLUSH_CALL_ERROR_MSG = "flush call on file stream failed";
constexpr const char *WRITE_CALL_ERROR_MSG = "write call on file stream failed";
constexpr const char *EMPTY_MESSAGE_ERROR_MSG = "empty message";
constexpr const char *SEEKG_CALL_ERROR_MSG = "seekg call on file stream failed";
constexpr const char *SEEKP_CALL_ERROR_MSG = "seekp call on file stream failed";

FileStream::FileStream(const std::string &path, bool append)
    : offset_(0),
      path_(path),
      logger_(logging::LoggerFactory<FileStream>::getLogger()) {
  file_stream_ = std::unique_ptr<std::fstream>(new std::fstream());
  if (append) {
    file_stream_->open(path.c_str(), std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::binary);
    if (file_stream_->is_open()) {
      seekToEndOfFile(FILE_OPENING_ERROR_MSG);
      auto len = file_stream_->tellg();
      if (len == std::streampos(-1))
        logging::LOG_ERROR(logger_) << FILE_OPENING_ERROR_MSG << TELLG_CALL_ERROR_MSG;
      length_ = len > 0 ? gsl::narrow<size_t>(len) : 0;
      seek(offset_);
    } else {
      logging::LOG_ERROR(logger_) << FILE_OPENING_ERROR_MSG << path << " " << strerror(errno);
    }
  } else {
    file_stream_->open(path.c_str(), std::fstream::out | std::fstream::binary);
    length_ = 0;
    if (!file_stream_->is_open()) {
      logging::LOG_ERROR(logger_) << FILE_OPENING_ERROR_MSG << path << " " << strerror(errno);
    }
  }
}

FileStream::FileStream(const std::string &path, uint32_t offset, bool write_enable)
    : offset_(offset),
      path_(path),
      logger_(logging::LoggerFactory<FileStream>::getLogger()) {
  file_stream_ = std::unique_ptr<std::fstream>(new std::fstream());
  if (write_enable) {
    file_stream_->open(path.c_str(), std::fstream::in | std::fstream::out | std::fstream::binary);
  } else {
    file_stream_->open(path.c_str(), std::fstream::in | std::fstream::binary);
  }
  if (file_stream_->is_open()) {
    seekToEndOfFile(FILE_OPENING_ERROR_MSG);
    auto len = file_stream_->tellg();
    if (len == std::streampos(-1))
      logging::LOG_ERROR(logger_) << FILE_OPENING_ERROR_MSG << TELLG_CALL_ERROR_MSG;
    length_ = len > 0 ? gsl::narrow<size_t>(len) : 0;
    seek(offset_);
  } else {
    logging::LOG_ERROR(logger_) << FILE_OPENING_ERROR_MSG << path << " " << strerror(errno);
  }
}

void FileStream::close() {
  std::lock_guard<std::mutex> lock(file_lock_);
  file_stream_.reset();
}

void FileStream::seek(size_t offset) {
  std::lock_guard<std::mutex> lock(file_lock_);
  if (file_stream_ == nullptr || !file_stream_->is_open()) {
    logging::LOG_ERROR(logger_) << SEEK_ERROR_MSG << INVALID_FILE_STREAM_ERROR_MSG;
    return;
  }
  offset_ = offset;
  file_stream_->clear();
  if (!file_stream_->seekg(offset_))
    logging::LOG_ERROR(logger_) << SEEK_ERROR_MSG << SEEKG_CALL_ERROR_MSG;
  if (!file_stream_->seekp(offset_))
    logging::LOG_ERROR(logger_) << SEEK_ERROR_MSG << SEEKP_CALL_ERROR_MSG;
}

int FileStream::write(const uint8_t *value, int size) {
  gsl_Expects(size >= 0);
  if (size == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(value)) {
    std::lock_guard<std::mutex> lock(file_lock_);
    if (file_stream_ == nullptr || !file_stream_->is_open()) {
      logging::LOG_ERROR(logger_) << WRITE_ERROR_MSG << INVALID_FILE_STREAM_ERROR_MSG;
      return -1;
    }
    if (file_stream_->write(reinterpret_cast<const char*>(value), size)) {
      offset_ += size;
      if (offset_ > length_) {
        length_ = offset_;
      }
      if (!file_stream_->flush()) {
        logging::LOG_ERROR(logger_) << WRITE_ERROR_MSG << FLUSH_CALL_ERROR_MSG;
        return -1;
      }
      return size;
    } else {
      logging::LOG_ERROR(logger_) << WRITE_ERROR_MSG << WRITE_CALL_ERROR_MSG;
      return -1;
    }
  } else {
    logging::LOG_ERROR(logger_) << WRITE_ERROR_MSG << EMPTY_MESSAGE_ERROR_MSG;
    return -1;
  }
}

size_t FileStream::read(uint8_t *buf, size_t buflen) {
  if (buflen == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(buf)) {
    std::lock_guard<std::mutex> lock(file_lock_);
    if (file_stream_ == nullptr || !file_stream_->is_open()) {
      logging::LOG_ERROR(logger_) << READ_ERROR_MSG << INVALID_FILE_STREAM_ERROR_MSG;
      return STREAM_ERROR;
    }
    file_stream_->read(reinterpret_cast<char*>(buf), gsl::narrow<std::streamsize>(buflen));
    if (file_stream_->eof() || file_stream_->fail()) {
      file_stream_->clear();
      seekToEndOfFile(READ_ERROR_MSG);
      auto tellg_result = file_stream_->tellg();
      if (tellg_result == std::streampos(-1)) {
        logging::LOG_ERROR(logger_) << READ_ERROR_MSG << TELLG_CALL_ERROR_MSG;
        return STREAM_ERROR;
      }
      const auto len = gsl::narrow<size_t>(tellg_result);
      size_t ret = len - offset_;
      offset_ = len;
      length_ = len;
      logging::LOG_DEBUG(logger_) << path_ << " eof bit, ended at " << offset_;
      return ret;
    } else {
      offset_ += buflen;
      file_stream_->seekp(gsl::narrow<std::streamoff>(offset_));
      return buflen;
    }
  } else {
    logging::LOG_ERROR(logger_) << READ_ERROR_MSG << INVALID_BUFFER_ERROR_MSG;
    return STREAM_ERROR;
  }
}

void FileStream::seekToEndOfFile(const char *caller_error_msg) {
  if (!file_stream_->seekg(0, file_stream_->end))
    logging::LOG_ERROR(logger_) << caller_error_msg << SEEKG_CALL_ERROR_MSG;
  if (!file_stream_->seekp(0, file_stream_->end))
    logging::LOG_ERROR(logger_) << caller_error_msg << SEEKP_CALL_ERROR_MSG;
}
} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

