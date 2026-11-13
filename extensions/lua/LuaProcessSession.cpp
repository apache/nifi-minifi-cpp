// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <memory>
#include <utility>

#include "LuaProcessSession.h"

namespace org::apache::nifi::minifi::extensions::lua {

LuaProcessSession::LuaProcessSession(core::ProcessSession& session)
    : session_(session) {
}

std::shared_ptr<LuaScriptFlowFile> LuaProcessSession::get() {
  auto flow_file = session_.get();

  if (flow_file == nullptr) {
    return nullptr;
  }

  auto result = std::make_shared<LuaScriptFlowFile>(flow_file);
  flow_files_.push_back(result);

  return result;
}

void LuaProcessSession::transfer(const std::shared_ptr<LuaScriptFlowFile>& script_flow_file,
                                 const core::Relationship& relationship) {
  const auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_.transfer(flow_file, relationship);
}

void LuaProcessSession::read(const std::shared_ptr<LuaScriptFlowFile> &script_flow_file,
                             sol::table input_stream_callback) {
  const auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_.read(flow_file, [&input_stream_callback](const std::shared_ptr<io::InputStream>& input_stream) -> io::IoResult {
    const int64_t callback_result = input_stream_callback["process"](input_stream_callback, std::make_shared<LuaInputStream>(input_stream));
    return io::IoResult::from(callback_result);
  });
}

void LuaProcessSession::write(const std::shared_ptr<LuaScriptFlowFile> &script_flow_file,
                              sol::table output_stream_callback) {
  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_.write(flow_file, [&output_stream_callback](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
    const int64_t callback_result = output_stream_callback["process"](output_stream_callback, std::make_shared<LuaOutputStream>(output_stream));
    return io::IoResult::from(callback_result);
  });
}

std::shared_ptr<LuaScriptFlowFile> LuaProcessSession::create() {
  auto result = std::make_shared<LuaScriptFlowFile>(session_.create());
  flow_files_.push_back(result);
  return result;
}

std::shared_ptr<LuaScriptFlowFile> LuaProcessSession::create(const std::shared_ptr<LuaScriptFlowFile> &flow_file) {
  std::shared_ptr<LuaScriptFlowFile> result;

  if (flow_file == nullptr) {
    result = std::make_shared<LuaScriptFlowFile>(session_.create());
  } else {
    result = std::make_shared<LuaScriptFlowFile>(session_.create(flow_file->getFlowFile().get()));
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
}

void LuaProcessSession::remove(const std::shared_ptr<LuaScriptFlowFile>& script_flow_file) {
  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_.remove(flow_file);
}

}  // namespace org::apache::nifi::minifi::extensions::lua
