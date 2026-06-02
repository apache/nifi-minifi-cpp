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

api::core::FlowFile MockProcessSession::create(const api::core::FlowFile*) {
  return api::core::FlowFile{new minifi_flow_file};
}
api::core::FlowFile MockProcessSession::get() {
  if (input_flow_files_.empty()) { return nullptr; }

  auto ff = std::move(input_flow_files_.back());
  input_flow_files_.pop_back();

  return api::core::FlowFile{ff.release()};
}

void MockProcessSession::penalize(api::core::FlowFile& ff) {
  ff->is_penalized = true;
}

void MockProcessSession::transfer(api::core::FlowFile ff, const minifi::core::Relationship& relationship) {
  transferred_flow_files_[relationship.getName()].push_back(std::unique_ptr<minifi_flow_file>(ff.release()));
}

void MockProcessSession::remove(api::core::FlowFile ff) {
  removed_flow_files_.push_back(std::unique_ptr<minifi_flow_file>(ff.release()));
}

void MockProcessSession::write(api::core::FlowFile& ff, const io::OutputStreamCallback& callback) {
  const auto stream = std::make_shared<MockOutputStream>(ff->content);
  callback(stream);
}

void MockProcessSession::read(api::core::FlowFile& ff, const io::InputStreamCallback& callback) {
  const auto stream = std::make_shared<MockInputStream>(ff->content);
  callback(stream);
}

void MockProcessSession::setAttribute(api::core::FlowFile& ff, std::string_view key, std::string value) {
  ff->attributes[std::string(key)] = std::move(value);
}

void MockProcessSession::removeAttribute(api::core::FlowFile& ff, std::string_view key) {
  ff->attributes.erase(std::string(key));
}

std::optional<std::string> MockProcessSession::getAttribute(api::core::FlowFile& ff, std::string_view key) {
  auto& attributes = ff->attributes;
  if (const auto it = attributes.find(std::string(key)); it != attributes.end()) { return it->second; }
  return std::nullopt;
}

std::map<std::string, std::string> MockProcessSession::getAttributes(const api::core::FlowFile& ff) const {
  return ff->attributes;
}

std::string MockProcessSession::getFlowFileId(const api::core::FlowFile& ff) const {
  return ff->id;
}

uint64_t MockProcessSession::getFlowFileSize(const api::core::FlowFile& ff) const {
  return ff->content.size();
}

void MockProcessSession::addInputFlowFile(std::unique_ptr<minifi_flow_file> flow_file) {
  input_flow_files_.push_back(std::move(flow_file));
}
}  // namespace org::apache::nifi::minifi::mock
