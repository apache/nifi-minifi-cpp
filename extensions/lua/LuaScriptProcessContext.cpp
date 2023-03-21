/**
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

#include <memory>
#include <string>
#include <utility>

#include "LuaScriptProcessContext.h"

namespace org::apache::nifi::minifi::extensions::lua {

LuaScriptProcessContext::LuaScriptProcessContext(std::shared_ptr<core::ProcessContext> context, sol::state& sol_state)
    : context_(std::move(context)), sol_state_(sol_state) {
}

std::string LuaScriptProcessContext::getProperty(const std::string &name) {
  std::string value;
  context_->getProperty(name, value);
  return value;
}

void LuaScriptProcessContext::releaseProcessContext() {
  context_.reset();
}

LuaScriptStateManager LuaScriptProcessContext::getStateManager() {
  return LuaScriptStateManager(context_->getStateManager(), sol_state_);
}

}  // namespace org::apache::nifi::minifi::extensions::lua
