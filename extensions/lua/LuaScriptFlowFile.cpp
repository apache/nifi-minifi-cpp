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

#include <utility>
#include <memory>
#include <string>

#include "minifi-cpp/core/FlowFile.h"

#include "LuaScriptFlowFile.h"

namespace org::apache::nifi::minifi::extensions::lua {

LuaScriptFlowFile::LuaScriptFlowFile(std::shared_ptr<core::FlowFile> flow_file)
    : flow_file_(std::move(flow_file)) {
}

std::string LuaScriptFlowFile::getAttribute(const std::string& key) {
  if (!flow_file_) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  std::string value;
  flow_file_->getAttribute(key, value);
  return value;
}

bool LuaScriptFlowFile::addAttribute(const std::string& key, const std::string& value) {
  if (!flow_file_) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  return flow_file_->addAttribute(key, value);
}

void LuaScriptFlowFile::setAttribute(const std::string& key, const std::string& value) {
  if (!flow_file_) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  flow_file_->setAttribute(key, value);
}

bool LuaScriptFlowFile::updateAttribute(std::string_view key, const std::string& value) {
  if (!flow_file_) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  return flow_file_->updateAttribute(key, value);
}

bool LuaScriptFlowFile::removeAttribute(std::string_view key) {
  if (!flow_file_) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  return flow_file_->removeAttribute(key);
}

std::shared_ptr<core::FlowFile> LuaScriptFlowFile::getFlowFile() {
  return flow_file_;
}

void LuaScriptFlowFile::releaseFlowFile() {
  flow_file_.reset();
}

}  // namespace org::apache::nifi::minifi::extensions::lua
