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

#include <mutex>
#include <utility>
#include <string>
#include <memory>
#include <vector>

#include "sol/sol.hpp"
#include "core/ProcessSession.h"

#include "LuaScriptEngine.h"
#include "LuaScriptProcessContext.h"
#include "LuaScriptException.h"
#include "LuaLogger.h"

#include "LuaProcessSession.h"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaScriptEngine {
 public:
  LuaScriptEngine();

  void eval(const std::string& script);
  void evalFile(const std::filesystem::path& file_name);
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session);
  void initialize(const core::Relationship& success, const core::Relationship& failure, const std::shared_ptr<core::logging::Logger>& logger);

  void setModulePaths(std::vector<std::filesystem::path> module_paths) {
    module_paths_ = std::move(module_paths);
  }

  template<typename... Args>
  void call(const std::string& fn_name, Args&& ...args) {
    sol::protected_function_result function_result{};
    try {
      if (sol::protected_function fn = lua_[fn_name.c_str()]) {
        function_result = fn(convert(args)...);
      }
    } catch (const std::exception& e) {
      throw LuaScriptException(e.what());
    }
    if (!function_result.valid()) {
      sol::error err = function_result;
      throw LuaScriptException(err.what());
    }
  }

  template<typename T>
  void bind(const std::string& name, const T& value) {
    lua_[name.c_str()] = convert(value);
  }

  template<typename T>
  T convert(const T& value) {
    return value;
  }

 private:
  void executeScriptWithAppendedModulePaths(std::string& script);

  std::unique_ptr<LuaLogger> lua_logger_;
  std::vector<std::filesystem::path> module_paths_;
  sol::state lua_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
