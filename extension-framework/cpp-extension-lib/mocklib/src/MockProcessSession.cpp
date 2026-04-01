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

#include "MockProcessSession.h"

#include "MockStreams.h"

namespace org::apache::nifi::minifi::mock {

MockProcessSession::~MockProcessSession() {
  // api::core::FlowFile requires that somebody takes ownership before it goes out of scope
  // the tested processor should either remove or transfer all FlowFiles it handles
  for (auto& ff : removed_flow_files_) {
    delete ff.release();  // NOLINT(cppcoreguidelines-owning-memory)
  }
  for (auto& ffs : transferred_flow_files_ | std::views::values) {
    for (auto& ff : ffs) {
      delete ff.release();  // NOLINT(cppcoreguidelines-owning-memory)
    }
  }
}

api::core::FlowFile MockProcessSession::create(const api::core::FlowFile*) {
  auto new_ff = api::core::FlowFile{new MinifiFlowFile};  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
  flow_file_datas[new_ff.get()];
  return new_ff;
}
api::core::FlowFile MockProcessSession::get() {
  if (input_flow_files_.empty()) { return nullptr; }

  auto ff = std::move(input_flow_files_.back());
  input_flow_files_.pop_back();
  flow_file_datas[ff.get()];

  return ff;
}

void MockProcessSession::penalize(api::core::FlowFile& ff) {
  auto& flow_file_data = flow_file_datas.at(ff.get());
  flow_file_data.is_penalized = true;
}

void MockProcessSession::transfer(api::core::FlowFile ff, const minifi::core::Relationship& relationship) {
  transferred_flow_files_[relationship.getName()].push_back(std::move(ff));
}

void MockProcessSession::remove(api::core::FlowFile ff) {
  removed_flow_files_.push_back(std::move(ff));
}

void MockProcessSession::write(api::core::FlowFile& ff, const io::OutputStreamCallback& callback) {
  auto& flow_file_data = flow_file_datas.at(ff.get());
  const auto stream = std::make_shared<MockOutputStream>(flow_file_data.content);
  callback(stream);
}

void MockProcessSession::read(api::core::FlowFile& ff, const io::InputStreamCallback& callback) {
  auto& flow_file_data = flow_file_datas.at(ff.get());
  const auto stream = std::make_shared<MockInputStream>(flow_file_data.content);
  callback(stream);
}

void MockProcessSession::setAttribute(api::core::FlowFile& ff, std::string_view key, std::string value) {
  auto& attributes = flow_file_datas.at(ff.get()).attributes;
  attributes[std::string(key)] = std::move(value);
}

void MockProcessSession::removeAttribute(api::core::FlowFile& ff, std::string_view key) {
  auto& attributes = flow_file_datas.at(ff.get()).attributes;
  attributes.erase(std::string(key));
}

std::optional<std::string> MockProcessSession::getAttribute(api::core::FlowFile& ff, std::string_view key) {
  auto& attributes = flow_file_datas.at(ff.get()).attributes;
  if (const auto it = attributes.find(std::string(key)); it != attributes.end()) { return it->second; }
  return std::nullopt;
}

std::map<std::string, std::string> MockProcessSession::getAttributes(const api::core::FlowFile& ff) const {
  return flow_file_datas.at(ff.get()).attributes;
}

std::string MockProcessSession::getFlowFileId(const api::core::FlowFile& ff) const {
  return flow_file_datas.at(ff.get()).id;
}

uint64_t MockProcessSession::getFlowFileSize(const api::core::FlowFile& ff) const {
  return flow_file_datas.at(ff.get()).content.size();
}

void MockProcessSession::addInputFlowFile(MockFlowFileData flow_file_data) {
  auto new_ff = api::core::FlowFile{new MinifiFlowFile};  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
  flow_file_datas.emplace(new_ff.get(), std::move(flow_file_data));
  input_flow_files_.push_back(std::move(new_ff));
}
}  // namespace org::apache::nifi::minifi::mock
