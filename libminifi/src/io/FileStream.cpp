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
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

constexpr const char *file_opening_error_msg = "Error opening file: ";
constexpr const char *read_error_msg = "Error reading from file: ";
constexpr const char *write_error_msg = "Error writing to file: ";
constexpr const char *seek_error = "Error seeking in file: ";
constexpr const char *invalid_file_stream_error_msg = "invalid file stream";
constexpr const char *tellg_call_error_msg = "tellg call on file stream failed";
constexpr const char *invalid_buffer_error_msg = "invalid buffer";
constexpr const char *flush_call_error_msg = "flush call on file stream failed";
constexpr const char *write_call_error_msg = "write call on file stream failed";
constexpr const char *empty_message_error_msg = "empty message";
constexpr const char *seekg_call_error_msg = "seekg call on file stream failed";
constexpr const char *seekp_call_error_msg = "seekp call on file stream failed";

FileStream::FileStream(const std::string &path, bool append)
    : logger_(logging::LoggerFactory<FileStream>::getLogger()),
      path_(path),
      offset_(0) {
  file_stream_ = std::unique_ptr<std::fstream>(new std::fstream());
  if (append) {
    file_stream_->open(path.c_str(), std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::binary);
    if (file_stream_->is_open()) {
      file_stream_->seekg(0, file_stream_->end);
      file_stream_->seekp(0, file_stream_->end);
      std::streamoff len = file_stream_->tellg();
      length_ = len > 0 ? gsl::narrow<size_t>(len) : 0;
      seek(offset_);
    } else {
      logging::LOG_ERROR(logger_) << file_opening_error_msg << path << " " << strerror(errno);
    }
  } else {
    file_stream_->open(path.c_str(), std::fstream::out | std::fstream::binary);
    length_ = 0;
    if (!file_stream_->is_open()) {
      logging::LOG_ERROR(logger_) << file_opening_error_msg << path << " " << strerror(errno);
    }
  }
}

FileStream::FileStream(const std::string &path, uint32_t offset, bool write_enable)
    : logger_(logging::LoggerFactory<FileStream>::getLogger()),
      path_(path),
      offset_(offset) {
  file_stream_ = std::unique_ptr<std::fstream>(new std::fstream());
  if (write_enable) {
    file_stream_->open(path.c_str(), std::fstream::in | std::fstream::out | std::fstream::binary);
  } else {
    file_stream_->open(path.c_str(), std::fstream::in | std::fstream::binary);
  }
  if (file_stream_->is_open()) {
    file_stream_->seekg(0, file_stream_->end);
    file_stream_->seekp(0, file_stream_->end);
    std::streamoff len = file_stream_->tellg();
    if (len > 0) {
      length_ = gsl::narrow<size_t>(len);
    } else {
      length_ = 0;
    }
    seek(offset);
  } else {
    logging::LOG_ERROR(logger_) << file_opening_error_msg << path << " " << strerror(errno);
  }
}

void FileStream::close() {
  std::lock_guard<std::mutex> lock(file_lock_);
  file_stream_.reset();
}

void FileStream::seek(uint64_t offset) {
  std::lock_guard<std::mutex> lock(file_lock_);
  if (file_stream_ == nullptr || !file_stream_->is_open()) {
    logging::LOG_ERROR(logger_) << seek_error << invalid_file_stream_error_msg;
    return;
  }
  offset_ = gsl::narrow<size_t>(offset);
  file_stream_->clear();
  if (!file_stream_->seekg(offset_))
    logging::LOG_ERROR(logger_) << seek_error << seekg_call_error_msg;
  if (!file_stream_->seekp(offset_))
    logging::LOG_ERROR(logger_) << seek_error << seekp_call_error_msg;
}

int FileStream::write(const uint8_t *value, int size) {
  gsl_Expects(size >= 0);
  if (size == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(value)) {
    std::lock_guard<std::mutex> lock(file_lock_);
    if (file_stream_ == nullptr || !file_stream_->is_open()) {
      logging::LOG_ERROR(logger_) << write_error_msg << invalid_file_stream_error_msg;
      return -1;
    }
    if (file_stream_->write(reinterpret_cast<const char*>(value), size)) {
      offset_ += size;
      if (offset_ > length_) {
        length_ = offset_;
      }
      if (!file_stream_->flush()) {
        logging::LOG_ERROR(logger_) << write_error_msg << flush_call_error_msg;
        return -1;
      }
      return size;
    } else {
      logging::LOG_ERROR(logger_) << write_error_msg << write_call_error_msg;
      return -1;
    }
  } else {
    logging::LOG_ERROR(logger_) << write_error_msg << empty_message_error_msg;
    return -1;
  }
}

int FileStream::read(uint8_t *buf, int buflen) {
  gsl_Expects(buflen >= 0);
  if (buflen == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(buf)) {
    std::lock_guard<std::mutex> lock(file_lock_);
    if (file_stream_ == nullptr || !file_stream_->is_open()) {
      logging::LOG_ERROR(logger_) << read_error_msg << invalid_file_stream_error_msg;
      return -1;
    }
    file_stream_->read(reinterpret_cast<char*>(buf), buflen);
    if (file_stream_->eof() || file_stream_->fail()) {
      file_stream_->clear();
      file_stream_->seekg(0, file_stream_->end);
      file_stream_->seekp(0, file_stream_->end);
      auto tellg_result = file_stream_->tellg();
      if (tellg_result < 0) {
        logging::LOG_ERROR(logger_) << read_error_msg << tellg_call_error_msg;
        return -1;
      }
      size_t len = gsl::narrow<size_t>(tellg_result);
      size_t ret = len - offset_;
      offset_ = len;
      length_ = len;
      logging::LOG_DEBUG(logger_) << path_ << " eof bit, ended at " << offset_;
      return gsl::narrow<int>(ret);
    } else {
      offset_ += buflen;
      file_stream_->seekp(offset_);
      return buflen;
    }

  } else {
    logging::LOG_ERROR(logger_) << read_error_msg << invalid_buffer_error_msg;
    return -1;
  }
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

