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
#pragma once

#include <map>
#include <span>

#include "IFlowFile.h"
#include "Relationship.h"
#include "minifi-cpp/io/StreamCallback.h"
#include "minifi-cpp/io/OutputStream.h"
#include "minifi-cpp/io/InputStream.h"

namespace org::apache::nifi::minifi::core {

class IProcessSession {
public:
  virtual void setAttribute(IFlowFile& ff, std::string_view key, std::string value) = 0;
  virtual void removeAttribute(IFlowFile& ff, std::string_view key) = 0;
  virtual std::optional<std::string> getAttribute(IFlowFile& ff, std::string_view key) = 0;
  virtual std::map<std::string, std::string> getAttributes(IFlowFile& ff) = 0;

  virtual std::shared_ptr<IFlowFile> create(const IFlowFile* parent) = 0;
  virtual std::shared_ptr<IFlowFile> popFlowFile() = 0;
  virtual void transfer(const std::shared_ptr<IFlowFile>& ff, const Relationship& relationship) = 0;
  virtual void write(IFlowFile& ff, const io::OutputStreamCallback& callback) = 0;
  virtual void read(IFlowFile& ff, const io::InputStreamCallback& callback) = 0;
  virtual ~IProcessSession() = default;

  void writeBuffer(const std::shared_ptr<IFlowFile>& flow_file, std::span<const char> buffer) {
    writeBuffer(flow_file, as_bytes(buffer));
  }

  void writeBuffer(const std::shared_ptr<IFlowFile>& flow_file, std::span<const std::byte> buffer) {
    write(*flow_file, [buffer](const std::shared_ptr<io::OutputStream>& output_stream) {
      const auto write_status = output_stream->write(buffer);
      return io::isError(write_status) ? -1 : gsl::narrow<int64_t>(write_status);
    });
  }

  std::vector<std::byte> readBuffer(IFlowFile& flow_file) {
    std::vector<std::byte> result;
    read(flow_file, [&result](const std::shared_ptr<io::InputStream>& input_stream) {
      result.resize(input_stream->size());
      const auto read_status = input_stream->read(result);
      return io::isError(read_status) ? -1 : gsl::narrow<int64_t>(read_status);
    });
    return result;
  }
};

}  // namespace org::apache::nifi::minifi::core
