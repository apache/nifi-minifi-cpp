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
#include "core/extension/Extension.h"
#include "utils/Environment.h"

class OpenCVExtension : org::apache::nifi::minifi::core::extension::Extension {
 public:
  using Extension::Extension;

  bool doInitialize(const std::shared_ptr<org::apache::nifi::minifi::Configure>& /*config*/) override {
    // By default in OpenCV, ffmpeg capture is hardcoded to use TCP and this is a workaround
    // also if UDP timeout, ffmpeg will retry with TCP
    // Note:
    // 1. OpenCV community are trying to find a better approach than setenv.
    // 2. The command will not overwrite value if "OPENCV_FFMPEG_CAPTURE_OPTIONS" already exists.
    return org::apache::nifi::minifi::utils::Environment::setEnvironmentVariable("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;udp", false /*overwrite*/);
  }

  void doDeinitialize() override {
  }
};

REGISTER_EXTENSION(OpenCVExtension);
