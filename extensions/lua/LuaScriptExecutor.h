// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <vector>
#include <filesystem>
#include <string>
#include <memory>
#include "../script/ScriptExecutor.h"
#include "LuaScriptEngine.h"
#include "utils/ResourceQueue.h"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaScriptExecutor : public script::ScriptExecutor {
 public:
  explicit LuaScriptExecutor(std::string_view name, const utils::Identifier& uuid = {});

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize(std::filesystem::path script_file,
      std::string script_body,
      std::optional<std::string> module_directory,
      size_t max_concurrent_engines,
      const core::Relationship& success,
      const core::Relationship& failure,
      const core::Relationship& original,
      const std::shared_ptr<core::logging::Logger>& logger) override;


  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
 private:
  std::shared_ptr<utils::ResourceQueue<LuaScriptEngine>> lua_script_engine_queue_;
};
}  // namespace org::apache::nifi::minifi::extensions::lua
