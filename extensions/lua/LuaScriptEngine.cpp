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
#include <string>
#include <filesystem>

#include "LuaScriptEngine.h"
#include "LuaProcessSession.h"
#include "LuaScriptProcessContext.h"
#include "LuaLogger.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::extensions::lua {


LuaScriptEngine::LuaScriptEngine() {
  lua_.open_libraries(sol::lib::base,
      sol::lib::os,
      sol::lib::coroutine,
      sol::lib::math,
      sol::lib::io,
      sol::lib::string,
      sol::lib::table,
      sol::lib::utf8,
      sol::lib::package);
  lua_.new_usertype<LuaLogger>(
      "Logger",
      "info", &LuaLogger::log_info);
  lua_.new_usertype<LuaProcessSession>(
      "ProcessSession",
      "create", static_cast<std::shared_ptr<LuaScriptFlowFile> (LuaProcessSession::*)()>(&LuaProcessSession::create),
      "get", &LuaProcessSession::get,
      "read", &LuaProcessSession::read,
      "write", &LuaProcessSession::write,
      "transfer", &LuaProcessSession::transfer,
      "remove", &LuaProcessSession::remove);
  lua_.new_usertype<LuaScriptFlowFile>(
      "FlowFile",
      "getAttribute", &LuaScriptFlowFile::getAttribute,
      "addAttribute", &LuaScriptFlowFile::addAttribute,
      "removeAttribute", &LuaScriptFlowFile::removeAttribute,
      "updateAttribute", &LuaScriptFlowFile::updateAttribute,
      "setAttribute", &LuaScriptFlowFile::setAttribute);
  lua_.new_usertype<LuaInputStream>(
      "InputStream",
      "read", &LuaInputStream::read);
  lua_.new_usertype<LuaOutputStream>(
      "OutputStream",
      "write", &LuaOutputStream::write);
  lua_.new_usertype<LuaScriptProcessContext>(
      "ProcessContext",
      "getStateManager", &LuaScriptProcessContext::getStateManager);
  lua_.new_usertype<LuaScriptStateManager>(
      "StateManager",
      "set", &LuaScriptStateManager::set,
      "get", &LuaScriptStateManager::get);
}

void LuaScriptEngine::executeScriptWithAppendedModulePaths(std::string& script) {
  for (const auto& module_path : module_paths_) {
    if (std::filesystem::is_regular_file(std::filesystem::status(module_path))) {
      script = utils::string::join_pack("package.path = package.path .. [[;", module_path.string(), "]]\n", script);
    } else {
      script = utils::string::join_pack("package.path = package.path .. [[;", module_path.string(), "/?.lua]]\n", script);
    }
  }
  lua_.script(script, sol::script_throw_on_error);
}

void LuaScriptEngine::eval(const std::string& script) {
  try {
    if (!module_paths_.empty()) {
      auto appended_script = script;
      executeScriptWithAppendedModulePaths(appended_script);
    } else {
      lua_.script(script, sol::script_throw_on_error);
    }
  } catch (std::exception& e) {
    throw LuaScriptException(e.what());
  }
}

void LuaScriptEngine::evalFile(const std::filesystem::path& file_name) {
  try {
    if (!module_paths_.empty()) {
      std::ifstream stream(file_name);
      std::string script((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
      executeScriptWithAppendedModulePaths(script);
    } else {
      lua_.script_file(file_name.string(), sol::script_throw_on_error);
    }
  } catch (std::exception& e) {
    throw LuaScriptException(e.what());
  }
}

void LuaScriptEngine::initialize(const core::Relationship& success, const core::Relationship& failure, const std::shared_ptr<core::logging::Logger>& logger) {
  lua_logger_ = std::make_unique<LuaLogger>(gsl::make_not_null(logger.get()));
  bind("log", lua_logger_.get());
  bind("REL_SUCCESS", success);
  bind("REL_FAILURE", failure);
}

namespace {
class TriggerSession {
 public:
  TriggerSession(std::shared_ptr<LuaScriptProcessContext> script_context,
      std::shared_ptr<LuaProcessSession> lua_session)
      : script_context_(std::move(script_context)),
      lua_session_(std::move(lua_session)) {
  }

  TriggerSession(TriggerSession&&) = delete;
  TriggerSession(const TriggerSession&) = delete;
  TriggerSession& operator=(TriggerSession&&) = delete;
  TriggerSession& operator=(const TriggerSession&) = delete;

  ~TriggerSession() {
    lua_session_->releaseCoreResources();
  }

 private:
  std::shared_ptr<LuaScriptProcessContext> script_context_;
  std::shared_ptr<LuaProcessSession> lua_session_;
};
}  // namespace

void LuaScriptEngine::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto script_context = std::make_shared<LuaScriptProcessContext>(context, lua_);
  auto lua_session = std::make_shared<LuaProcessSession>(session);
  TriggerSession trigger_session(script_context, lua_session);
  call("onTrigger", script_context, lua_session);
}

}  // namespace org::apache::nifi::minifi::extensions::lua
