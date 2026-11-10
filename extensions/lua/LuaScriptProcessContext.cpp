// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <memory>
#include <string>
#include <utility>

#include "LuaScriptProcessContext.h"

namespace org::apache::nifi::minifi::extensions::lua {

LuaScriptProcessContext::LuaScriptProcessContext(core::ProcessContext& context, sol::state& sol_state)
    : context_(context), sol_state_(sol_state) {
}

std::string LuaScriptProcessContext::getProperty(const std::string &name) {
  return context_.getProperty(name).value_or("");
}

LuaScriptStateManager LuaScriptProcessContext::getStateManager() {
  return LuaScriptStateManager(context_.getStateManager(), sol_state_);
}

}  // namespace org::apache::nifi::minifi::extensions::lua
