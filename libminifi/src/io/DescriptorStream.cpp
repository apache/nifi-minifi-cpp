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
    : fd_(fd),
      logger_(logging::LoggerFactory<DescriptorStream>::getLogger()) {
}

void DescriptorStream::seek(uint64_t offset) {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
#ifdef WIN32
  _lseeki64(fd_, gsl::narrow<int64_t>(offset), 0x00);
#else
  lseek(fd_, gsl::narrow<off_t>(offset), 0x00);
#endif
}

int DescriptorStream::write(const uint8_t *value, int size) {
  gsl_Expects(size >= 0);
  if (size == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(value)) {
    std::lock_guard<std::recursive_mutex> lock(file_lock_);
#ifdef WIN32
    if (_write(fd_, value, size) != size) {
#else
    if (::write(fd_, value, size) != size) {
#endif
      return -1;
    } else {
      return size;
    }
  } else {
    return -1;
  }
}

int DescriptorStream::read(uint8_t *buf, int buflen) {
  gsl_Expects(buflen >= 0);
  if (buflen == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(buf)) {
#ifdef WIN32
    auto size_read = _read(fd_, buf, buflen);
#else
    auto size_read = ::read(fd_, buf, buflen);
#endif

    if (size_read < 0) {
      return -1;
    }
    return  size_read;

  } else {
    return -1;
  }
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

