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

#include <concepts>
#include <algorithm>
#include <string>
#include <memory>
#include <utility>
#include <optional>

#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"

#include "ScriptExecutor.h"
#include "utils/Enum.h"
#include "utils/ResourceQueue.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::processors {

namespace execute_script {
enum class ScriptEngineOption {
  lua,
  python
};
}  // namespace execute_script

class ExecuteScript : public core::ProcessorImpl {
 public:
  explicit ExecuteScript(std::string_view name, const utils::Identifier &uuid = {})
      : ProcessorImpl(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Executes a script given the flow file and a process session. "
      "The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) as well as "
      "any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context "
      "and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state "
      "if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the "
      "concurrent tasks to 1.";

  EXTENSIONAPI static constexpr auto ScriptEngine = core::PropertyDefinitionBuilder<2>::createProperty("Script Engine")
      .withDescription(R"(The engine to execute scripts (python, lua))")
      .isRequired(true)
      .withAllowedValues(magic_enum::enum_names<execute_script::ScriptEngineOption>())
      .withDefaultValue(magic_enum::enum_name(execute_script::ScriptEngineOption::python))
      .build();
  EXTENSIONAPI static constexpr auto ScriptFile = core::PropertyDefinitionBuilder<>::createProperty("Script File")
      .withDescription(R"(Path to script file to execute. Only one of Script File or Script Body may be used)")
      .build();
  EXTENSIONAPI static constexpr auto ScriptBody = core::PropertyDefinitionBuilder<>::createProperty("Script Body")
      .withDescription(R"(Body of script to execute. Only one of Script File or Script Body may be used)")
      .build();
  EXTENSIONAPI static constexpr auto ModuleDirectory = core::PropertyDefinitionBuilder<>::createProperty("Module Directory")
      .withDescription(R"(Comma-separated list of paths to files and/or directories which contain modules required by the script)")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      ScriptEngine,
      ScriptFile,
      ScriptBody,
      ModuleDirectory
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Script successes"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Script failures"};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original", "Original flow file"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure, Original};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExecuteScript>::getLogger(uuid_);

  std::unique_ptr<extensions::script::ScriptExecutor> script_executor_;
};

}  // namespace org::apache::nifi::minifi::processors
