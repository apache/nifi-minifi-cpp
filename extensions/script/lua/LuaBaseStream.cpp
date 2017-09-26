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

#include "LuaBaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace lua {

LuaBaseStream::LuaBaseStream(std::shared_ptr<io::BaseStream> stream)
    : stream_(std::move(stream)) {
}

std::string LuaBaseStream::read(size_t len) {
  if (len == 0) {
    len = stream_->getSize();
  }

  if (len <= 0) {
    return nullptr;
  }

  std::string buffer;
  buffer.resize(len);

  // Here, we write directly to the string data, which is safe starting in C++ 11:
  //
  //     The char-like objects in a basic_string object shall be stored contiguously. That is, for any basic_string
  //     object s, the identity &*(s.begin() + n) == &*s.begin() + n shall hold for all values of n such that
  //     0 <= n < s.size()."
  //
  // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3337.pdf
  auto read = stream_->readData(reinterpret_cast<uint8_t *>(&buffer[0]), static_cast<int>(len));

  if (read != len) {
    buffer.resize(static_cast<size_t >(read));
  }

  return std::move(buffer);
}

size_t LuaBaseStream::write(std::string buf) {
  return static_cast<size_t>(stream_->writeData(reinterpret_cast<uint8_t *>(const_cast<char *>(buf.data())),
                                                static_cast<int>(buf.length())));
}

} /* namespace lua */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
