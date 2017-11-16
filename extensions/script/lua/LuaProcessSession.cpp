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

#include <memory>

#include "LuaProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace lua {

LuaProcessSession::LuaProcessSession(std::shared_ptr<core::ProcessSession> session)
    : session_(std::move(session)) {
}

std::shared_ptr<script::ScriptFlowFile> LuaProcessSession::get() {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = session_->get();

  if (flow_file == nullptr) {
    return nullptr;
  }

  auto result = std::make_shared<script::ScriptFlowFile>(flow_file);
  flow_files_.push_back(result);

  return result;
}

void LuaProcessSession::transfer(const std::shared_ptr<script::ScriptFlowFile> &script_flow_file,
                                 core::Relationship relationship) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->transfer(flow_file, relationship);
}

void LuaProcessSession::read(const std::shared_ptr<script::ScriptFlowFile> &script_flow_file,
                             sol::table input_stream_callback) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  LuaInputStreamCallback lua_callback(input_stream_callback);
  session_->read(flow_file, &lua_callback);
}

void LuaProcessSession::write(const std::shared_ptr<script::ScriptFlowFile> &script_flow_file,
                              sol::table output_stream_callback) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  LuaOutputStreamCallback lua_callback(output_stream_callback);
  session_->write(flow_file, &lua_callback);
}

std::shared_ptr<script::ScriptFlowFile> LuaProcessSession::create() {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto result = std::make_shared<script::ScriptFlowFile>(session_->create());
  flow_files_.push_back(result);
  return result;
}

std::shared_ptr<script::ScriptFlowFile> LuaProcessSession::create(const std::shared_ptr<script::ScriptFlowFile> &flow_file) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  std::shared_ptr<script::ScriptFlowFile> result;

  if (flow_file == nullptr) {
    result = std::make_shared<script::ScriptFlowFile>(session_->create());
  } else {
    result = std::make_shared<script::ScriptFlowFile>(session_->create(flow_file->getFlowFile()));
  }

  flow_files_.push_back(result);
  return result;
}

void LuaProcessSession::releaseCoreResources() {
  for (const auto &flow_file : flow_files_) {
    if (flow_file) {
      flow_file->releaseFlowFile();
    }
  }

  session_.reset();
}

} /* namespace lua */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
