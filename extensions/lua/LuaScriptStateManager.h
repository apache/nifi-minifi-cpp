// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>

#include "sol/sol.hpp"
#include "minifi-cpp/core/StateManager.h"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaScriptStateManager {
 public:
  explicit LuaScriptStateManager(core::StateManager* state_manager, sol::state& sol_state) : state_manager_(state_manager), sol_state_(sol_state) {}

  bool set(const sol::table& core_component_state_lua);
  sol::optional<sol::table> get();

 private:
  core::StateManager* state_manager_;
  sol::state& sol_state_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
