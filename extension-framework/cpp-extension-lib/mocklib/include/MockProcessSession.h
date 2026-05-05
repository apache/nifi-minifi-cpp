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

namespace org::apache::nifi::minifi::mock {
struct MockFlowFileData {
  explicit MockFlowFileData(std::string content_str) {
    this->content = content_str
           | ranges::views::transform([](char c) { return static_cast<std::byte>(c); })
           | ranges::to<std::vector<std::byte>>();
  }

  ~MockFlowFileData() = default;

  MockFlowFileData() = default;
  MockFlowFileData(const MockFlowFileData&) = default;
  MockFlowFileData(MockFlowFileData&&) = default;
  MockFlowFileData& operator=(const MockFlowFileData&) = default;
  MockFlowFileData& operator=(MockFlowFileData&&) = default;

  std::map<std::string, std::string> attributes;
  std::vector<std::byte> content;
  bool is_penalized = false;
  std::string id;
};

class MockProcessSession : public api::core::ProcessSession {
 public:
  MockProcessSession() = default;
  MockProcessSession(const MockProcessSession&) = delete;
  MockProcessSession& operator=(const MockProcessSession&) = delete;
  MockProcessSession(MockProcessSession&&) = delete;
  MockProcessSession& operator=(MockProcessSession&&) = delete;

  ~MockProcessSession() override;

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

  void addInputFlowFile(MockFlowFileData flow_file_data);

 private:
  std::vector<api::core::FlowFile> input_flow_files_;
  std::map<MinifiFlowFile*, MockFlowFileData> flow_file_datas;
  std::map<std::string, std::vector<api::core::FlowFile>> transferred_flow_files_;
  std::vector<api::core::FlowFile> removed_flow_files_;
};

}  // namespace org::apache::nifi::minifi::mock
