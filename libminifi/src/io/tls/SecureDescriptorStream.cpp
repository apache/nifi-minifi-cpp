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
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

SecureDescriptorStream::SecureDescriptorStream(int fd, SSL *ssl)
    : fd_(fd), ssl_(ssl),
      logger_(logging::LoggerFactory<SecureDescriptorStream>::getLogger()) {
}

void SecureDescriptorStream::seek(uint64_t offset) {
  std::lock_guard<std::recursive_mutex> lock(file_lock_);
#ifdef WIN32
  _lseeki64(fd_, gsl::narrow<int64_t>(offset), 0x00);
#else
  lseek(fd_, gsl::narrow<off_t>(offset), 0x00);
#endif
}

// data stream overrides

int SecureDescriptorStream::write(const uint8_t *value, int size) {
  gsl_Expects(size >= 0);
  if (size == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(value)) {
    std::lock_guard<std::recursive_mutex> lock(file_lock_);
    int bytes = 0;
     int sent = 0;
     while (bytes < size) {
       sent = SSL_write(ssl_, value + bytes, size - bytes);
       // check for errors
       if (sent < 0) {
         int ret = 0;
         ret = SSL_get_error(ssl_, sent);
         logger_->log_error("WriteData socket %d send failed %s %d", fd_, strerror(errno), ret);
         return sent;
       }
       bytes += sent;
     }
     return size;
  } else {
    return -1;
  }
}

int SecureDescriptorStream::read(uint8_t *buf, int buflen) {
  gsl_Expects(buflen >= 0);
  if (buflen == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(buf)) {
    int total_read = 0;
      int status = 0;
      while (buflen) {
        int sslStatus;
        do {
          status = SSL_read(ssl_, buf, buflen);
          sslStatus = SSL_get_error(ssl_, status);
        } while (status < 0 && sslStatus == SSL_ERROR_WANT_READ);

        if (status < 0)
              break;

        buflen -= status;
        buf += status;
        total_read += status;
      }

      return total_read;

  } else {
    return -1;
  }
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

