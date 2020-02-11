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

#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class WriteCallback : public OutputStreamCallback {
public:
  WriteCallback(const char *data, uint64_t size)
    : _data(const_cast<char*>(data)),
    _dataSize(size) {
  }
  char *_data;
  uint64_t _dataSize;
  int64_t process(std::shared_ptr<io::BaseStream> stream) {
    int64_t ret = 0;
    if (_data && _dataSize > 0)
	  ret = stream->write(reinterpret_cast<uint8_t*>(_data), _dataSize);
    return ret;
  }
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
