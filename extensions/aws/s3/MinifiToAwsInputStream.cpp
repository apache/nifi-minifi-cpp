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
#include "MinifiToAwsInputStream.h"

#include <algorithm>
#include <span>

namespace org::apache::nifi::minifi::aws::s3 {

MinifiInputStreamBuf::int_type MinifiInputStreamBuf::underflow() {
  if (gptr() < egptr()) {
    return traits_type::to_int_type(*gptr());
  }
  const uint64_t stream_pos = stream_->tell();
  if (stream_pos >= start_pos_ + content_length_) {
    return traits_type::eof();
  }
  const auto remaining = (start_pos_ + content_length_) - stream_pos;
  const auto to_read = std::min<uint64_t>(utils::configuration::DEFAULT_BUFFER_SIZE, remaining);
  const auto bytes_read = stream_->read(std::span(reinterpret_cast<std::byte*>(buffer_.data()), gsl::narrow<size_t>(to_read)));
  if (io::isError(bytes_read)) {
    owner_->setstate(std::ios_base::badbit);
    return traits_type::eof();
  }
  if (bytes_read == 0) {
    return traits_type::eof();
  }
  setg(buffer_.data(), buffer_.data(), buffer_.data() + bytes_read);
  return traits_type::to_int_type(*gptr());
}

MinifiInputStreamBuf::pos_type MinifiInputStreamBuf::seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which) {
  if (!(which & std::ios_base::in)) {
    return pos_type(off_type(-1));
  }
  pos_type new_virtual_pos;
  if (way == std::ios_base::beg) {
    new_virtual_pos = pos_type(off);
  } else if (way == std::ios_base::cur) {
    const auto phys_pos = static_cast<off_type>(stream_->tell()) - static_cast<off_type>(egptr() - gptr());
    new_virtual_pos = pos_type(phys_pos - static_cast<off_type>(start_pos_) + off);
  } else {
    new_virtual_pos = pos_type(static_cast<off_type>(content_length_) + off);
  }
  return seekpos(new_virtual_pos, which);
}

MinifiInputStreamBuf::pos_type MinifiInputStreamBuf::seekpos(pos_type pos, std::ios_base::openmode which) {
  if (!(which & std::ios_base::in)) {
    return pos_type(off_type(-1));
  }
  stream_->seek(start_pos_ + static_cast<size_t>(off_type(pos)));
  setg(buffer_.data(), buffer_.data(), buffer_.data());  // invalidate read buffer
  return pos;
}

}  // namespace org::apache::nifi::minifi::aws::s3
