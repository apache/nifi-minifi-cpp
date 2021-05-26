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

#include "io/tls/SecureDescriptorStream.h"
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <Exception.h>
#include "io/validation.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

SecureDescriptorStream::SecureDescriptorStream(int fd, SSL *ssl)
    : fd_(fd), ssl_(ssl),
      logger_(logging::LoggerFactory<SecureDescriptorStream>::getLogger()) {
}

void SecureDescriptorStream::seek(size_t offset) {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
#ifdef WIN32
  _lseeki64(fd_, gsl::narrow<int64_t>(offset), 0x00);
#else
  lseek(fd_, gsl::narrow<off_t>(offset), 0x00);
#endif
}

// data stream overrides

size_t SecureDescriptorStream::write(const uint8_t *value, size_t size) {
  if (size == 0) {
    return 0;
  }
  if (IsNullOrEmpty(value)) return STREAM_ERROR;
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
  size_t bytes = 0;
  while (bytes < size) {
    const auto write_status = SSL_write(ssl_, value + bytes, gsl::narrow_cast<int>(size - bytes));
    // check for errors
    if (write_status < 0) {
      const auto ret = SSL_get_error(ssl_, write_status);
      logger_->log_error("WriteData socket %d send failed %s %d", fd_, strerror(errno), ret);
      return STREAM_ERROR;
    }
    bytes += gsl::narrow<size_t>(write_status);
  }
  return size;
}

size_t SecureDescriptorStream::read(uint8_t * const buf, const size_t buflen) {
  if (buflen == 0) {
    return 0;
  }
  if (IsNullOrEmpty(buf)) return STREAM_ERROR;
  size_t total_read = 0;
  uint8_t* writepos = buf;
  while (buflen > total_read) {
    int status;
    int sslStatus;
    do {
      const auto ssl_read_size = gsl::narrow<int>(std::min(buflen - total_read, gsl::narrow<size_t>(std::numeric_limits<int>::max())));
      status = SSL_read(ssl_, writepos, ssl_read_size);
      sslStatus = SSL_get_error(ssl_, status);
    } while (status < 0 && sslStatus == SSL_ERROR_WANT_READ);

    if (status < 0)
      break;

    writepos += status;
    total_read += gsl::narrow<size_t>(status);
  }

  return total_read;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

