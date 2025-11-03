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

#include "minifi-c.h"
#include "minifi-cpp/core/Relationship.h"
#include "minifi-cpp/io/StreamCallback.h"
#include "FlowFile.h"

namespace org::apache::nifi::minifi::api::core {

class ProcessSession {
 public:
  explicit ProcessSession(MinifiProcessSession* impl): impl_(impl) {}

  std::shared_ptr<FlowFile> create(const FlowFile* parent = nullptr);
  std::shared_ptr<FlowFile> get();
  void transfer(const std::shared_ptr<FlowFile>& ff, const minifi::core::Relationship& relationship);
  void write(FlowFile &flow, const io::OutputStreamCallback& callback);
  void read(FlowFile &flow, const io::InputStreamCallback& callback);

  void setAttribute(FlowFile& ff, std::string_view key, std::string value);
  void removeAttribute(FlowFile& ff, std::string_view key);
  std::optional<std::string> getAttribute(FlowFile& ff, std::string_view key);
  std::map<std::string, std::string> getAttributes(FlowFile& ff);

  void writeBuffer(const std::shared_ptr<FlowFile>& flow_file, std::span<const char> buffer);
  void writeBuffer(const std::shared_ptr<FlowFile>& flow_file, std::span<const std::byte> buffer);
  std::vector<std::byte> readBuffer(FlowFile& flow_file);

 private:
  MinifiProcessSession* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
