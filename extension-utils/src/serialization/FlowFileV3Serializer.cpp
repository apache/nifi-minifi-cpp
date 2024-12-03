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
#include "io/StreamPipe.h"
#include "utils/gsl.h"
#include "core/FlowFile.h"

namespace org::apache::nifi::minifi {

size_t FlowFileV3Serializer::writeLength(std::size_t length, const std::shared_ptr<io::OutputStream>& out) {
  if (length < MAX_2_BYTE_VALUE) {
    return out->write(static_cast<uint16_t>(length));
  }
  size_t sum = 0;
  {
    const auto ret = out->write(static_cast<uint16_t>(MAX_2_BYTE_VALUE));
    if (io::isError(ret)) return ret;
    sum += ret;
  }
  {
    const auto ret = out->write(static_cast<uint32_t>(length));
    if (io::isError(ret)) return ret;
    sum += ret;
  }
  return sum;
}

size_t FlowFileV3Serializer::writeString(const std::string &str, const std::shared_ptr<io::OutputStream> &out) {
  size_t sum = 0;
  {
    const auto ret = writeLength(str.length(), out);
    if (io::isError(ret)) return ret;
    sum += ret;
  }
  {
    const auto ret = out->write(reinterpret_cast<const uint8_t*>(str.data()), str.length());
    if (io::isError(ret)) return ret;
    if (ret != str.length()) return io::STREAM_ERROR;
    sum += ret;
  }
  return sum;
}

int64_t FlowFileV3Serializer::serialize(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<io::OutputStream>& out) {
  size_t sum = 0;
  {
    const auto ret = out->write(MAGIC_HEADER, sizeof(MAGIC_HEADER));
    if (io::isError(ret)) return -1;
    if (ret != sizeof(MAGIC_HEADER)) return -1;
    sum += ret;
  }
  const auto& attributes = flowFile->getAttributes();
  {
    const auto ret = writeLength(attributes.size(), out);
    if (io::isError(ret)) return -1;
    sum += ret;
  }
  for (const auto& attrIt : attributes) {
    {
      const auto ret = writeString(attrIt.first, out);
      if (io::isError(ret)) return -1;
      sum += ret;
    }
    {
      const auto ret = writeString(attrIt.second, out);
      if (io::isError(ret)) return -1;
      sum += ret;
    }
  }
  {
    const auto ret = out->write(static_cast<uint64_t>(flowFile->getSize()));
    if (io::isError(ret)) return -1;
    sum += ret;
  }
  {
    const auto ret = reader_(flowFile, InputStreamPipe{*out});
    if (ret < 0) return -1;
    sum += gsl::narrow<size_t>(ret);
  }
  return gsl::narrow<int64_t>(sum);
}

}  // namespace org::apache::nifi::minifi
