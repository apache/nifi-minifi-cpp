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
#include <ranges>

#include "MockUtils.h"
#include "api/core/ProcessSession.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"

struct minifi_flow_file {
  explicit minifi_flow_file(std::string content_str) {
    this->content = content_str
           | ranges::views::transform([](char c) { return static_cast<std::byte>(c); })
           | ranges::to<std::vector<std::byte>>();
  }

  ~minifi_flow_file() = default;

  minifi_flow_file() = default;
  minifi_flow_file(const minifi_flow_file&) = default;
  minifi_flow_file(minifi_flow_file&&) = default;
  minifi_flow_file& operator=(const minifi_flow_file&) = default;
  minifi_flow_file& operator=(minifi_flow_file&&) = default;

  std::map<std::string, std::string> attributes;
  std::vector<std::byte> content;
  bool is_penalized = false;
  std::string id;
};

namespace org::apache::nifi::minifi::mock {

class MockProcessSession : public api::core::ProcessSession {
 public:
  MockProcessSession() = default;
  MockProcessSession(const MockProcessSession&) = delete;
  MockProcessSession& operator=(const MockProcessSession&) = delete;
  MockProcessSession(MockProcessSession&&) = delete;
  MockProcessSession& operator=(MockProcessSession&&) = delete;

  api::core::FlowFile create(const api::core::FlowFile* parent) override;
  api::core::FlowFile get() override;
  void penalize(api::core::FlowFile& ff) override;
  void transfer(api::core::FlowFile ff, const minifi::core::Relationship& relationship) override;
  void remove(api::core::FlowFile ff) override;
  void write(api::core::FlowFile& ff, const io::OutputStreamCallback& callback) override;
  void read(api::core::FlowFile& ff, const io::InputStreamCallback& callback) override;
  void setAttribute(api::core::FlowFile& ff, std::string_view key, std::string value) override;
  void removeAttribute(api::core::FlowFile& ff, std::string_view key) override;
  std::optional<std::string> getAttribute(api::core::FlowFile& ff, std::string_view key) override;
  [[nodiscard]] std::map<std::string, std::string> getAttributes(const api::core::FlowFile& ff) const override;
  [[nodiscard]] std::string getFlowFileId(const api::core::FlowFile& ff) const override;
  [[nodiscard]] uint64_t getFlowFileSize(const api::core::FlowFile& ff) const override;

  void addInputFlowFile(std::unique_ptr<minifi_flow_file> flow_file);

 private:
  std::vector<std::unique_ptr<minifi_flow_file>> input_flow_files_;
  using RelationshipName = std::string;
  std::map<RelationshipName, std::vector<std::unique_ptr<minifi_flow_file>>> transferred_flow_files_;
  std::vector<std::unique_ptr<minifi_flow_file>> removed_flow_files_;
};

}  // namespace org::apache::nifi::minifi::mock
