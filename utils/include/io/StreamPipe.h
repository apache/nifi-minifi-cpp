/**
 * @file FlowFileRecord.h
 * Flow file record class declaration
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

#include <functional>
#include <memory>
#include <utility>
#include "InputStream.h"
#include "OutputStream.h"
#include "StreamCallback.h"

namespace org::apache::nifi::minifi {
namespace internal {

inline int64_t pipe(io::InputStream& src, io::OutputStream& dst) {
  std::array<std::byte, 4096> buffer{};
  int64_t totalTransferred = 0;
  while (true) {
    const auto readRet = src.read(buffer);
    if (io::isError(readRet)) return -1;
    if (readRet == 0) break;
    auto remaining = readRet;
    size_t transferred = 0;
    while (remaining > 0) {
      const auto writeRet = dst.write(gsl::make_span(buffer).subspan(transferred, remaining));
      // TODO(adebreceni):
      //   write might return 0, e.g. in case of a congested server
      //   what should we return then?
      //     - the number of bytes read or
      //     - the number of bytes wrote
      if (io::isError(writeRet)) {
        return -1;
      }
      transferred += writeRet;
      remaining -= writeRet;
    }
    totalTransferred += transferred;
  }
  return totalTransferred;
}

}  // namespace internal

class InputStreamPipe {
 public:
  explicit InputStreamPipe(io::OutputStream& output) : output_(&output) {}

  int64_t operator()(const std::shared_ptr<io::InputStream>& stream) const {
    return internal::pipe(*stream, *output_);
  }

 private:
  gsl::not_null<io::OutputStream*> output_;
};

class OutputStreamPipe {
 public:
  explicit OutputStreamPipe(io::InputStream& input) : input_(&input) {}

  int64_t operator()(const std::shared_ptr<io::OutputStream>& stream) const {
    return internal::pipe(*input_, *stream);
  }

 private:
  gsl::not_null<io::InputStream*> input_;
};

}  // namespace org::apache::nifi::minifi
