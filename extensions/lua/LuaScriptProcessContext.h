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

#pragma once

#include <string>
#include <memory>

#include "core/ProcessSession.h"
#include "LuaScriptStateManager.h"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaScriptProcessContext {
 public:
  explicit LuaScriptProcessContext(std::shared_ptr<core::ProcessContext> context, sol::state& sol_state);

  std::string getProperty(const std::string &name);
  void releaseProcessContext();

  LuaScriptStateManager getStateManager();

 private:
  std::shared_ptr<core::ProcessContext> context_;
  sol::state& sol_state_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
