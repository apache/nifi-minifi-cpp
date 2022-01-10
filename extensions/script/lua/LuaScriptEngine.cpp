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

#include "../ScriptProcessContext.h"

#include "LuaScriptEngine.h"
#include "LuaProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace lua {

LuaScriptEngine::LuaScriptEngine()
    : lua_() {
  lua_.open_libraries(sol::lib::base,
                      sol::lib::os,
                      sol::lib::coroutine,
                      sol::lib::math,
                      sol::lib::io,
                      sol::lib::string,
                      sol::lib::table,
                      sol::lib::utf8,
                      sol::lib::package);
  lua_.new_usertype<core::logging::Logger>(
      "Logger",
      "info", &core::logging::Logger::log_info<>);
  lua_.new_usertype<lua::LuaProcessSession>(
      "ProcessSession",
      "create", static_cast<std::shared_ptr<script::ScriptFlowFile> (lua::LuaProcessSession::*)()>(&lua::LuaProcessSession::create),
      "get", &lua::LuaProcessSession::get,
      "read", &lua::LuaProcessSession::read,
      "write", &lua::LuaProcessSession::write,
      "transfer", &lua::LuaProcessSession::transfer);
  lua_.new_usertype<script::ScriptFlowFile>(
      "FlowFile",
      "getAttribute", &script::ScriptFlowFile::getAttribute,
      "addAttribute", &script::ScriptFlowFile::addAttribute,
      "removeAttribute", &script::ScriptFlowFile::removeAttribute,
      "updateAttribute", &script::ScriptFlowFile::updateAttribute,
      "setAttribute", &script::ScriptFlowFile::setAttribute);
  lua_.new_usertype<lua::LuaBaseStream>(
      "BaseStream",
      "read", &lua::LuaBaseStream::read,
      "write", &lua::LuaBaseStream::write);
}

void LuaScriptEngine::eval(const std::string &script) {
  try {
    if (!module_directories_.empty()) {
      auto appended_script = script;
      for (const auto& module_dir : module_directories_) {
        appended_script = "package.path = package.path .. \";" + module_dir + "/?.lua\"\n" + script;
      }
      lua_.script(appended_script, sol::script_throw_on_error);
    } else {
      lua_.script(script, sol::script_throw_on_error);
    }
  } catch (std::exception& e) {
    throw minifi::script::ScriptException(e.what());
  }
}

void LuaScriptEngine::evalFile(const std::string &file_name) {
  try {
    if (!module_directories_.empty()) {
      std::ifstream stream(file_name);
      std::string script((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
      for (const auto& module_dir : module_directories_) {
        script = "package.path = package.path .. \";" + module_dir + "/?.lua\"\n" + script;
      }
      lua_.script(script, sol::script_throw_on_error);
    } else {
      lua_.script_file(file_name, sol::script_throw_on_error);
    }
  } catch (std::exception& e) {
    throw minifi::script::ScriptException(e.what());
  }
}

} /* namespace lua */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
