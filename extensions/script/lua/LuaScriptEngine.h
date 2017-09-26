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

#ifndef NIFI_MINIFI_CPP_LUASCRIPTENGINE_H
#define NIFI_MINIFI_CPP_LUASCRIPTENGINE_H

#include <mutex>
#include <sol.hpp>
#include <core/ProcessSession.h>

#include "../ScriptEngine.h"
#include "../ScriptProcessContext.h"

#include "LuaProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace lua {

class LuaScriptEngine : public script::ScriptEngine {
 public:
  LuaScriptEngine();

  void eval(const std::string &script) override;
  void evalFile(const std::string &file_name) override;

  /**
   * Calls the given function, forwarding arbitrary provided parameters.
   *
   * @return
   */
  template<typename... Args>
  void call(const std::string &fn_name, Args &&...args) {
    sol::function fn = lua_[fn_name.c_str()];
    fn(convert(args)...);
  }


  template<typename T>
  void bind(const std::string &name, const T &value) {
    lua_[name.c_str()] = convert(value);
  }

  template<typename T>
  T convert(const T &value) {
    return value;
  }

  std::shared_ptr<LuaProcessSession> convert(const std::shared_ptr<core::ProcessSession> &session) {
    return std::make_shared<LuaProcessSession>(session);
  }

  std::shared_ptr<script::ScriptProcessContext> convert(const std::shared_ptr<core::ProcessContext> &context) {
    return std::make_shared<script::ScriptProcessContext>(context);
  }

 private:
  sol::state lua_;
};

} /* namespace lua */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_LUASCRIPTENGINE_H
