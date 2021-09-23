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

#include "io/StreamSlice.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

StreamSlice::StreamSlice(std::shared_ptr<io::BaseStream>& stream, size_t offset, size_t size) : stream_(stream), slice_offset_(offset), slice_size_(size) {
  stream_->seek(slice_offset_);
  if (stream_->size() < slice_offset_ + slice_size_)
    throw std::invalid_argument("StreamSlice is bigger than the Stream");
}

size_t StreamSlice::read(uint8_t *value, size_t len) {
  const size_t max_size = std::min(len, size() - tell());
  return stream_->read(value, max_size);
}

void StreamSlice::seek(size_t offset) {
  stream_->seek(slice_offset_ + offset);
}

size_t StreamSlice::tell() const {
  return stream_->tell() - slice_offset_;
}

const uint8_t* StreamSlice::getBuffer() const {
  const uint8_t* buffer = stream_->getBuffer();
  return buffer + slice_offset_;
}

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
