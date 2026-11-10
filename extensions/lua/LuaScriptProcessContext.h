// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>

#include "core/ProcessSession.h"
#include "LuaScriptStateManager.h"
#include "minifi-cpp/core/ProcessContext.h"

#include "sol/sol.hpp"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaScriptProcessContext {
 public:
  LuaScriptProcessContext(core::ProcessContext& context, sol::state& sol_state);

  std::string getProperty(const std::string &name);
  LuaScriptStateManager getStateManager();

 private:
  core::ProcessContext& context_;
  sol::state& sol_state_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
