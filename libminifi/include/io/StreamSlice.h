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

#pragma once
#include <algorithm>
#include <memory>

#include "BaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * A wrapped Base Stream with configurable offset and size
 * It hides the original (bigger stream) and acts like the stream starts and ends at the configured offset/size
 */
class StreamSlice : public BaseStream {  // TODO(MINIFICPP-1648) This should be an InputStreamCallback, because writing to Slice is not supported
 public:
  StreamSlice(std::shared_ptr<io::BaseStream>& stream, size_t offset, size_t size);

  // from InputStream
  size_t size() const override { return slice_size_; }
  size_t read(uint8_t *value, size_t len) override;

  // from OutputStream
  size_t write(const uint8_t*, size_t) override { throw std::runtime_error("write is not supported in StreamSlice"); }

  // from Stream
  void close() override { stream_->close(); }
  int initialize() override { return stream_->initialize(); }

  void seek(size_t offset) override;
  size_t tell() const override;
  const uint8_t* getBuffer() const override;

 private:
  const std::shared_ptr<io::BaseStream>& stream_;
  size_t slice_offset_;
  size_t slice_size_;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
