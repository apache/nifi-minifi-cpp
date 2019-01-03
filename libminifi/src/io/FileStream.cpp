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

#include "io/FileStream.h"
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

FileStream::FileStream(const std::string &path)
    : logger_(logging::LoggerFactory<FileStream>::getLogger()),
      path_(path),
      offset_(0) {
  file_stream_ = std::unique_ptr<std::fstream>(new std::fstream());
  file_stream_->open(path.c_str(), std::fstream::out | std::fstream::binary);
  file_stream_->seekg(0, file_stream_->end);
  file_stream_->seekp(0, file_stream_->end);
  int len = file_stream_->tellg();
  if (len > 0) {
    length_ = len;
  } else {
    length_ = 0;
  }
  seek(offset_);
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
  file_stream_->seekg(0, file_stream_->end);
  file_stream_->seekp(0, file_stream_->end);
  int len = file_stream_->tellg();
  if (len > 0) {
    length_ = len;
  } else {
    length_ = 0;
  }
  seek(offset);
}

void FileStream::closeStream() {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
  if (file_stream_ != nullptr) {
    file_stream_->close();
    file_stream_ = nullptr;
  }
}

void FileStream::seek(uint64_t offset) {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
  offset_ = offset;
  file_stream_->clear();
  file_stream_->seekg(offset_);
  file_stream_->seekp(offset_);
}

int FileStream::writeData(std::vector<uint8_t> &buf, int buflen) {
  if (static_cast<int>(buf.capacity()) < buflen) {
    return -1;
  }
  return writeData(reinterpret_cast<uint8_t *>(&buf[0]), buflen);
}

// data stream overrides

int FileStream::writeData(uint8_t *value, int size) {
  if (!IsNullOrEmpty(value)) {
    std::lock_guard<std::recursive_mutex> lock(file_lock_);
    if (file_stream_->write(reinterpret_cast<const char*>(value), size)) {
      offset_ += size;
      if (offset_ > length_) {
        length_ = offset_;
      }
      file_stream_->seekg(offset_);
      file_stream_->flush();
      return size;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

template<typename T>
inline std::vector<uint8_t> FileStream::readBuffer(const T& t) {
  std::vector<uint8_t> buf;
  buf.resize(sizeof t);
  readData(reinterpret_cast<uint8_t *>(&buf[0]), sizeof(t));
  return buf;
}

int FileStream::readData(std::vector<uint8_t> &buf, int buflen) {
  if (static_cast<int>(buf.capacity()) < buflen) {
    buf.resize(buflen);
  }
  int ret = readData(reinterpret_cast<uint8_t*>(&buf[0]), buflen);

  if (ret < buflen) {
    buf.resize(ret);
  }
  return ret;
}

int FileStream::readData(uint8_t *buf, int buflen) {
  if (!IsNullOrEmpty(buf)) {
    std::lock_guard<std::recursive_mutex> lock(file_lock_);
    if (!file_stream_) {
      return -1;
    }
    file_stream_->read(reinterpret_cast<char*>(buf), buflen);
    if ((file_stream_->rdstate() & (file_stream_->eofbit | file_stream_->failbit)) != 0) {
      file_stream_->clear();
      file_stream_->seekg(0, file_stream_->end);
      file_stream_->seekp(0, file_stream_->end);
      int len = file_stream_->tellg();
      size_t ret = len - offset_;
      offset_ = len;
      length_ = len;
      logging::LOG_DEBUG(logger_) << path_ << " eof bit, ended at " << offset_;
      return ret;
    } else {
      offset_ += buflen;
      file_stream_->seekp(offset_);
      return buflen;
    }

  } else {
    return -1;
  }
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

