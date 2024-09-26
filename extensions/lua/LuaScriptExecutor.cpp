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

#include <string>
#include <filesystem>

#include "LuaScriptExecutor.h"
#include "range/v3/range/conversion.hpp"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::lua {

LuaScriptExecutor::LuaScriptExecutor(std::string_view name, const utils::Identifier& uuid) : script::ScriptExecutor(name, uuid) {}

void LuaScriptExecutor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  const auto lua_script_engine = lua_script_engine_queue_->getResource();
  gsl_Expects(std::holds_alternative<std::filesystem::path>(script_to_run_) || std::holds_alternative<std::string>(script_to_run_));

  if (module_directory_) {
    lua_script_engine->setModulePaths(utils::string::splitAndTrimRemovingEmpty(*module_directory_, ",") | ranges::to<std::vector<std::filesystem::path>>());
  }

  if (std::holds_alternative<std::filesystem::path>(script_to_run_))
    lua_script_engine->evalFile(std::get<std::filesystem::path>(script_to_run_));
  else
    lua_script_engine->eval(std::get<std::string>(script_to_run_));

  lua_script_engine->onTrigger(context, session);
}

void LuaScriptExecutor::initialize(std::filesystem::path script_file,
    std::string script_body,
    std::optional<std::string> module_directory,
    size_t max_concurrent_engines,
    const core::Relationship& success,
    const core::Relationship& failure,
    const core::Relationship& /*original*/,
    const std::shared_ptr<core::logging::Logger>& logger) {
  if (script_file.empty() == script_body.empty())
    throw std::runtime_error("Exactly one of these must be non-empty: ScriptBody, ScriptFile");

  if (!script_file.empty()) {
    script_to_run_.emplace<std::filesystem::path>(std::move(script_file));
  }
  if (!script_body.empty()) {
    script_to_run_.emplace<std::string>(std::move(script_body));
  }

  module_directory_ = std::move(module_directory);

  auto create_engine = [=]() -> std::unique_ptr<LuaScriptEngine> {
    auto engine = std::make_unique<LuaScriptEngine>();
    engine->initialize(success, failure, logger);
    return engine;
  };

  lua_script_engine_queue_ = utils::ResourceQueue<LuaScriptEngine>::create(create_engine, max_concurrent_engines, std::nullopt, logger);
}

REGISTER_RESOURCE(LuaScriptExecutor, InternalResource);

}  // namespace org::apache::nifi::minifi::extensions::lua
