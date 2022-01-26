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
#include <Exception.h>
#include "io/validation.h"

#ifndef WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

DescriptorStream::DescriptorStream(int fd)
    : fd_(fd) {
}

void DescriptorStream::seek(size_t offset) {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
#ifdef WIN32
  _lseeki64(fd_, gsl::narrow<int64_t>(offset), SEEK_SET);
#else
  lseek(fd_, gsl::narrow<off_t>(offset), SEEK_SET);
#endif
}

size_t DescriptorStream::tell() const {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
#ifdef WIN32
  return _lseeki64(fd_, 0, SEEK_CUR);
#else
  return lseek(fd_, 0, SEEK_CUR);
#endif
}

size_t DescriptorStream::write(const uint8_t *value, size_t size) {
  if (size == 0) return 0;
  if (IsNullOrEmpty(value)) return STREAM_ERROR;
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
#ifdef WIN32
  if (static_cast<size_t>(_write(fd_, value, size)) != size) {
#else
  if (static_cast<size_t>(::write(fd_, value, size)) != size) {
#endif
    return STREAM_ERROR;
  } else {
    return size;
  }
}

size_t DescriptorStream::read(gsl::span<std::byte> buf) {
  if (buf.empty()) return 0;
#ifdef WIN32
  const auto size_read = _read(fd_, buf.data(), buf.size());
#else
  const auto size_read = ::read(fd_, buf.data(), buf.size());
#endif

  if (size_read < 0) {
    return STREAM_ERROR;
  }
  return gsl::narrow<size_t>(size_read);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

