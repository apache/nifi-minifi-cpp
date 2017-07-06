/**
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
#ifndef LIBMINIFI_INCLUDE_UTILS_BYTEINPUTCALLBACK_H_
#define LIBMINIFI_INCLUDE_UTILS_BYTEINPUTCALLBACK_H_

#include <fstream>
#include <iterator>
#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * General vector based uint8_t callback.
 */
class ByteInputCallBack : public InputStreamCallback {
 public:
  ByteInputCallBack()
      : ptr(nullptr) {
  }

  virtual ~ByteInputCallBack() {

  }

  int64_t process(std::shared_ptr<io::BaseStream> stream) {

    stream->seek(0);

    if (stream->getSize() > 0) {
      vec.resize(stream->getSize());

      stream->readData(vec, stream->getSize());
    }

    ptr = (char*) &vec[0];

    return vec.size();

  }

  char *getBuffer() {
    return ptr;
  }

  const size_t getBufferSize() {
    return vec.size();
  }

 private:
  char *ptr;
  std::vector<uint8_t> vec;
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_BYTEINPUTCALLBACK_H_ */
