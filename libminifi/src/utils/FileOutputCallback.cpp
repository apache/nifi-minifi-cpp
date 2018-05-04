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
#include "utils/FileOutputCallback.h"
#include <vector>
#include <utility>
#include <string>
#include <memory>
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

int64_t FileOutputCallback::process(std::shared_ptr<io::BaseStream> stream) {
  if (stream->getSize() > 0) {
    file_stream_.write(reinterpret_cast<char*>(const_cast<uint8_t*>(stream->getBuffer())), stream->getSize());
    size_ += stream->getSize();
  }
  return size_.load();
}

const std::vector<char> FileOutputCallback::to_string() {
  std::vector<char> buffer;
  buffer.insert(std::end(buffer), std::begin(file_), std::end(file_));
  return buffer;
}

void FileOutputCallback::close() {
  is_alive_ = false;
  file_stream_.close();
}

size_t FileOutputCallback::getSize() {
  return size_;
}

void FileOutputCallback::write(char *data, size_t size) {
  file_stream_.write(data, size);
  size_ += size;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
