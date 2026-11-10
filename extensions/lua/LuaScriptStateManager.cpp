// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "LuaScriptStateManager.h"

namespace org::apache::nifi::minifi::extensions::lua {

namespace {
core::StateManager::State from_lua(const sol::table& lua_state) {
  core::StateManager::State cpp_state;
  for (const auto& [lua_state_key, lua_state_value] : lua_state)
    cpp_state[lua_state_key.as<std::string>()] = lua_state_value.as<std::string>();
  return cpp_state;
}

sol::table to_lua(const core::StateManager::State& cpp_state, sol::state& sol_state) {
  auto lua_state = sol::table(sol_state.lua_state(), sol::create);
  for (const auto& [cpp_state_key, cpp_state_value] : cpp_state)
    lua_state[cpp_state_key] = cpp_state_value;
  return lua_state;
}
}  // namespace

bool LuaScriptStateManager::set(const sol::table& core_component_state_lua) {
  if (!state_manager_)
    return false;

  return state_manager_->set(from_lua(core_component_state_lua));
}

sol::optional<sol::table> LuaScriptStateManager::get() {
  if (!state_manager_)
    return sol::nullopt;
  if (auto core_component_state_cpp = state_manager_->get())
    return to_lua(*core_component_state_cpp, sol_state_);
  return sol::nullopt;
}

}  // namespace org::apache::nifi::minifi::extensions::lua
