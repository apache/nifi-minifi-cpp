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

#include "serialization/FlowFileV3Serializer.h"
#include "core/ProcessSession.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

constexpr uint8_t FlowFileV3Serializer::MAGIC_HEADER[];

int FlowFileV3Serializer::writeLength(std::size_t length, const std::shared_ptr<io::OutputStream>& out) {
  if (length < MAX_2_BYTE_VALUE) {
    return out->write(static_cast<uint16_t>(length));
  }
  int sum = 0;
  int ret;
  ret = out->write(static_cast<uint16_t>(MAX_2_BYTE_VALUE));
  if (ret < 0) {
    return ret;
  }
  sum += ret;
  ret = out->write(static_cast<uint32_t>(length));
  if (ret < 0) {
    return ret;
  }
  sum += ret;
  return sum;
}

int FlowFileV3Serializer::writeString(const std::string &str, const std::shared_ptr<io::OutputStream> &out) {
  int sum = 0;
  int ret;
  ret = writeLength(str.length(), out);
  if (ret < 0) {
    return ret;
  }
  sum += ret;
  ret = out->write(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(str.data())), gsl::narrow<int>(str.length()));
  if (ret < 0) {
    return ret;
  }
  if (gsl::narrow<size_t>(ret) != str.length()) {
    return -1;
  }
  sum += ret;
  return sum;
}

int FlowFileV3Serializer::serialize(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<io::OutputStream>& out) {
  int sum = 0;
  int ret;
  ret = out->write(const_cast<uint8_t*>(MAGIC_HEADER), sizeof(MAGIC_HEADER));
  if (ret < 0) {
    return ret;
  }
  if (ret != sizeof(MAGIC_HEADER)) {
    return -1;
  }
  sum += ret;
  const auto& attributes = flowFile->getAttributes();
  ret = writeLength(attributes.size(), out);
  if (ret < 0) {
    return ret;
  }
  sum += ret;
  for (const auto& attrIt : attributes) {
    ret = writeString(attrIt.first, out);
    if (ret < 0) {
      return ret;
    }
    sum += ret;
    ret = writeString(attrIt.second, out);
    if (ret < 0) {
      return ret;
    }
    sum += ret;
  }
  ret = out->write(static_cast<uint64_t>(flowFile->getSize()));
  if (ret < 0) {
    return ret;
  }
  sum += ret;
  InputStreamPipe pipe(out);
  ret = reader_(flowFile, &pipe);
  if (ret < 0) {
    return ret;
  }
  sum += ret;
  return sum;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
