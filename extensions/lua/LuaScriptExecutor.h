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

#include <vector>
#include <filesystem>
#include <string>
#include <memory>
#include <utility>
#include "../script/ScriptExecutor.h"
#include "LuaScriptEngine.h"
#include "utils/ResourceQueue.h"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaScriptExecutor : public script::ScriptExecutor {
 public:
  explicit LuaScriptExecutor(std::string name, const utils::Identifier& uuid = {});

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize(std::filesystem::path script_file,
      std::string script_body,
      std::optional<std::string> module_directory,
      size_t max_concurrent_engines,
      const core::Relationship& success,
      const core::Relationship& failure,
      const std::shared_ptr<core::logging::Logger>& logger) override;


  static auto properties() { return std::array<core::Property, 0>{}; }
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
 private:
  std::shared_ptr<utils::ResourceQueue<LuaScriptEngine>> lua_script_engine_queue_;
};
}  // namespace org::apache::nifi::minifi::extensions::lua
