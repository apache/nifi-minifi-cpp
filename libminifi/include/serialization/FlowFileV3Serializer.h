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

#include <limits>
#include <string>
#include <memory>
#include "io/OutputStream.h"
#include "FlowFileSerializer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class FlowFileV3Serializer : public FlowFileSerializer {
  static constexpr uint8_t MAGIC_HEADER[] = {'N', 'i', 'F', 'i', 'F', 'F', '3'};

  static constexpr uint16_t MAX_2_BYTE_VALUE = (std::numeric_limits<uint16_t>::max)();

  static size_t writeLength(std::size_t length, const std::shared_ptr<io::OutputStream>& out);

  static size_t writeString(const std::string& str, const std::shared_ptr<io::OutputStream>& out);

 public:
  using FlowFileSerializer::FlowFileSerializer;

  int64_t serialize(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<io::OutputStream>& out) override;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
