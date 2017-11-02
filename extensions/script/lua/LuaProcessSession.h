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

#ifndef NIFI_MINIFI_CPP_LUAPROCESSSESSION_H
#define NIFI_MINIFI_CPP_LUAPROCESSSESSION_H

#include <sol.hpp>

#include <core/ProcessSession.h>

#include "../ScriptFlowFile.h"

#include "LuaBaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace lua {

class LuaProcessSession {
 public:
  explicit LuaProcessSession(std::shared_ptr<core::ProcessSession> session);

  std::shared_ptr<script::ScriptFlowFile> get();
  std::shared_ptr<script::ScriptFlowFile> create();
  std::shared_ptr<script::ScriptFlowFile> create(const std::shared_ptr<script::ScriptFlowFile> &flow_file);
  void transfer(const std::shared_ptr<script::ScriptFlowFile> &flow_file, core::Relationship relationship);
  void read(const std::shared_ptr<script::ScriptFlowFile> &flow_file, sol::table input_stream_callback);
  void write(const std::shared_ptr<script::ScriptFlowFile> &flow_file, sol::table output_stream_callback);

  class LuaInputStreamCallback : public InputStreamCallback {
   public:
    explicit LuaInputStreamCallback(const sol::table &input_stream_callback) {
      lua_callback_ = input_stream_callback;
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) override {
      auto lua_stream = std::make_shared<LuaBaseStream>(stream);
      sol::function callback = lua_callback_["process"];
      return callback(lua_callback_, lua_stream);
    }

   private:
    sol::table lua_callback_;
  };

  class LuaOutputStreamCallback : public OutputStreamCallback {
   public:
    explicit LuaOutputStreamCallback(const sol::table &output_stream_callback) {
      lua_callback_ = output_stream_callback;
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) override {
      auto lua_stream = std::make_shared<LuaBaseStream>(stream);
      sol::function callback = lua_callback_["process"];
      return callback(lua_callback_, lua_stream);
    }

   private:
    sol::table lua_callback_;
  };

 private:
  std::shared_ptr<core::ProcessSession> session_;
};

} /* namespace lua */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_LUAPROCESSSESSION_H
