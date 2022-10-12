/**
 * @file ExecuteScript.h
 * ExecuteScript class declaration
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

#pragma once

#include <string>
#include <memory>
#include <utility>
#include <optional>

#include "concurrentqueue.h"
#include "core/Processor.h"

#include "ScriptEngine.h"
#include "ScriptProcessContext.h"
#include "utils/Enum.h"
#include "utils/ResourceQueue.h"

#ifdef LUA_SUPPORT
#include "lua/LuaScriptEngine.h"
#endif  // LUA_SUPPORT

#ifdef PYTHON_SUPPORT
#include "python/PythonScriptEngine.h"
#endif  // PYTHON_SUPPORT

namespace org::apache::nifi::minifi::processors {

class ScriptEngineFactory {
 public:
  ScriptEngineFactory(const core::Relationship& success, const core::Relationship& failure, std::shared_ptr<core::logging::Logger> logger);

  template<typename T>
  std::enable_if_t<std::is_base_of_v<script::ScriptEngine, T>, std::unique_ptr<T>> createEngine() const {
    auto engine = std::make_unique<T>();

    engine->bind("log", logger_);
    engine->bind("REL_SUCCESS", success_);
    engine->bind("REL_FAILURE", failure_);

    return engine;
  }

 private:
  const core::Relationship& success_;
  const core::Relationship& failure_;
  std::shared_ptr<core::logging::Logger> logger_;
};

class ExecuteScript : public core::Processor {
 public:
  SMART_ENUM(ScriptEngineOption,
    (LUA, "lua"),
    (PYTHON, "python")
  )

  explicit ExecuteScript(std::string name, const utils::Identifier &uuid = {})
      : Processor(std::move(name), uuid),
        engine_factory_(Success, Failure, logger_) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Executes a script given the flow file and a process session. "
      "The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) as well as "
      "any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context "
      "and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state "
      "if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the "
      "concurrent tasks to 1.";

  EXTENSIONAPI static const core::Property ScriptEngine;
  EXTENSIONAPI static const core::Property ScriptFile;
  EXTENSIONAPI static const core::Property ScriptBody;
  EXTENSIONAPI static const core::Property ModuleDirectory;
  static auto properties() {
    return std::array{
      ScriptEngine,
      ScriptFile,
      ScriptBody,
      ModuleDirectory
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
    logger_->log_error("onTrigger invocation with raw pointers is not implemented");
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                 const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExecuteScript>::getLogger();

  ScriptEngineOption script_engine_;
  std::string script_file_;
  std::string script_body_;
  std::optional<std::string> module_directory_;

  ScriptEngineFactory engine_factory_;
#ifdef LUA_SUPPORT
  std::shared_ptr<utils::ResourceQueue<lua::LuaScriptEngine>> lua_script_engine_queue_;
#endif  // LUA_SUPPORT
#ifdef PYTHON_SUPPORT
  std::unique_ptr<python::PythonScriptEngine> python_script_engine_;
#endif  // PYTHON_SUPPORT
};

}  // namespace org::apache::nifi::minifi::processors
