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
#include "minifi-cpp/core/extension/ExtensionInfo.h"
#include "minifi-cpp/properties/Configure.h"
#include "utils/Environment.h"
#include "minifi-cpp/agent/agent_version.h"

namespace minifi = org::apache::nifi::minifi;

extern "C" minifi::core::extension::ExtensionInitializer InitExtension = [] (const std::shared_ptr<minifi::Configure>& /*config*/) -> std::optional<minifi::core::extension::ExtensionInfo> {
  // By default in OpenCV, ffmpeg capture is hardcoded to use TCP and this is a workaround
  // also if UDP timeout, ffmpeg will retry with TCP
  // Note:
  // 1. OpenCV community are trying to find a better approach than setenv.
  // 2. The command will not overwrite value if "OPENCV_FFMPEG_CAPTURE_OPTIONS" already exists.
  const auto success = org::apache::nifi::minifi::utils::Environment::setEnvironmentVariable("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;udp", false /*overwrite*/);
  if (!success) {
    return std::nullopt;
  }
  return minifi::core::extension::ExtensionInfo{
    .name = "OpenCVExtension",
    .version = minifi::AgentBuild::VERSION,
    .deinit = nullptr,
    .ctx = nullptr
  };
};
