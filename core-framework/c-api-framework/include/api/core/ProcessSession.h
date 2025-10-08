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
#include "minifi-cpp/core/Relationship.h"
#include "minifi-cpp/io/StreamCallback.h"
#include "minifi-cpp/core/IProcessSession.h"
#include "FlowFile.h"

namespace org::apache::nifi::minifi::api::core {

class ProcessSession : public minifi::core::IProcessSession {
 public:
  explicit ProcessSession(MinifiProcessSession impl): impl_(impl) {}

  std::shared_ptr<minifi::core::IFlowFile> create(const minifi::core::IFlowFile* parent) override;
  std::shared_ptr<FlowFile> create(const FlowFile* parent = nullptr) {
    return std::dynamic_pointer_cast<FlowFile>(create(static_cast<const minifi::core::IFlowFile*>(parent)));
  }
  std::shared_ptr<minifi::core::IFlowFile> popFlowFile() override;
  std::shared_ptr<FlowFile> get() {
    return std::dynamic_pointer_cast<FlowFile>(popFlowFile());
  }
  void transfer(const std::shared_ptr<minifi::core::IFlowFile>& ff, const minifi::core::Relationship& relationship) override;
  void write(minifi::core::IFlowFile &flow, const io::OutputStreamCallback& callback) override;
  void read(minifi::core::IFlowFile &flow, const io::InputStreamCallback& callback) override;

  void setAttribute(minifi::core::IFlowFile& ff, std::string_view key, std::string value) override;
  void removeAttribute(minifi::core::IFlowFile& ff, std::string_view key) override;
  std::optional<std::string> getAttribute(minifi::core::IFlowFile& ff, std::string_view key) override;
  std::map<std::string, std::string> getAttributes(minifi::core::IFlowFile& ff) override;

 private:
  MinifiProcessSession impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
