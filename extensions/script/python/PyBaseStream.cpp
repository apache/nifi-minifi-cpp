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

#include <stdexcept>
#include <memory>
#include <utility>
#include <string>
#include <vector>

#include "PyBaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

PyBaseStream::PyBaseStream(std::shared_ptr<io::BaseStream> stream)
    : stream_(std::move(stream)) {
}

py::bytes PyBaseStream::read() {
  return read(stream_->size());
}

py::bytes PyBaseStream::read(size_t len) {
  if (len == 0) {
    len = stream_->size();
  }

  if (len <= 0) {
    return nullptr;
  }

  std::vector<uint8_t> buffer(len);

  const auto read = stream_->read(buffer.data(), len);
  return py::bytes(reinterpret_cast<char *>(buffer.data()), read);
}

size_t PyBaseStream::write(const py::bytes& buf) {
  auto buf_str = buf.operator std::string();
  return stream_->write(reinterpret_cast<const uint8_t*>(buf_str.data()), buf_str.length());
}

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
