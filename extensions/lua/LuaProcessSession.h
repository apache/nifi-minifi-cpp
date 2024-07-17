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
#include <memory>

#include "core/ProcessSession.h"
#include "LuaScriptFlowFile.h"

#include "sol/sol.hpp"
#include "LuaInputStream.h"
#include "LuaOutputStream.h"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaProcessSession {
 public:
  explicit LuaProcessSession(core::ProcessSession& session);

  std::shared_ptr<LuaScriptFlowFile> get();
  std::shared_ptr<LuaScriptFlowFile> create();
  std::shared_ptr<LuaScriptFlowFile> create(const std::shared_ptr<LuaScriptFlowFile>& script_flow_file);
  void transfer(const std::shared_ptr<LuaScriptFlowFile>& script_flow_file, const core::Relationship& relationship);
  void read(const std::shared_ptr<LuaScriptFlowFile>& script_flow_file, sol::table input_stream_callback);
  void write(const std::shared_ptr<LuaScriptFlowFile>& script_flow_file, sol::table output_stream_callback);
  void remove(const std::shared_ptr<LuaScriptFlowFile>& script_flow_file);

  /**
   * Sometimes we want to release shared pointers to core resources when
   * we know they are no longer in need. This method is for those times.
   *
   * For example, we do not want to hold on to shared pointers to FlowFiles
   * after an onTrigger call, because doing so can be very expensive in terms
   * of repository resources.
   */
  void releaseCoreResources();

 private:
  std::vector<std::shared_ptr<LuaScriptFlowFile>> flow_files_;
  core::ProcessSession& session_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
