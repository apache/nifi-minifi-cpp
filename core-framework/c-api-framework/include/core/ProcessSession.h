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

#include "minifi-c.h"
#include "FlowFile.h"
#include "minifi-cpp/core/Relationship.h"
#include <span>
#include "minifi-cpp/io/StreamCallback.h"

namespace org::apache::nifi::minifi::core {

class ProcessSession {
public:
  explicit ProcessSession(MinifiProcessSession impl): impl_(impl) {}

  std::shared_ptr<FlowFile> create(const FlowFile* parent = nullptr);
  std::shared_ptr<FlowFile> get();
  void transfer(const std::shared_ptr<FlowFile>& ff, const Relationship& relationship);
  void writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const char> buffer);
  void writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const std::byte> buffer);
  void write(core::FlowFile &flow, const io::OutputStreamCallback& callback);
  void read(core::FlowFile &flow, const io::InputStreamCallback& callback);
  std::vector<std::byte> readBuffer(core::FlowFile &flow);

private:
  MinifiProcessSession impl_;
};

}  // namespace org::apache::nifi::minifi::cpp::core
