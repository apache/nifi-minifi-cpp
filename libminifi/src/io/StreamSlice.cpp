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

namespace org::apache::nifi::minifi::io {

StreamSlice::StreamSlice(std::shared_ptr<io::InputStream> stream, size_t offset, size_t size) : stream_(std::move(stream)), slice_offset_(offset), slice_size_(size) {
  stream_->seek(slice_offset_);
  if (stream_->size() < slice_offset_ + slice_size_)
    throw std::invalid_argument("StreamSlice is bigger than the Stream");
}

size_t StreamSlice::read(gsl::span<std::byte> out_buffer) {
  const size_t max_size = std::min(out_buffer.size(), size() - tell());
  return stream_->read(out_buffer.subspan(0, max_size));
}

void StreamSlice::seek(size_t offset) {
  stream_->seek(slice_offset_ + offset);
}

size_t StreamSlice::tell() const {
  return stream_->tell() - slice_offset_;
}

gsl::span<const std::byte> StreamSlice::getBuffer() const {
  return stream_->getBuffer().subspan(slice_offset_, slice_size_);
}

}  // namespace org::apache::nifi::minifi::io
