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

#include <string>

#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class WriteCallback : public OutputStreamCallback {
public:
  explicit WriteCallback(std::string data)
    : data_(std::move(data)) {
  }

 int64_t process(const std::shared_ptr<io::BaseStream>& stream) {
    if (data_.empty())
      return 0;

    return stream->write(reinterpret_cast<uint8_t*>(const_cast<char*>(data_.c_str())), data_.size());
  }

 std::string data_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
