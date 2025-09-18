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
#include <map>

#include "properties/Configure.h"
#include "utils/StringUtils.h"
#include "minifi-cpp/io/ArchiveStream.h"
#include "utils/expected.h"
#include "io/BufferStream.h"

namespace org::apache::nifi::minifi::c2 {

inline constexpr const char* UPDATE_NAME = "C2UpdatePolicy";
inline constexpr const char* C2_METRICS_PUBLISHER = "C2MetricsPublisher";
inline constexpr const char* CONTROLLER_SOCKET_METRICS_PUBLISHER = "ControllerSocketMetricsPublisher";

bool isC2Enabled(const std::shared_ptr<Configure>& configuration);
bool isControllerSocketEnabled(const std::shared_ptr<Configure>& configuration);
nonstd::expected<std::shared_ptr<io::BufferStream>, std::string> createDebugBundleArchive(const std::map<std::string, std::unique_ptr<io::InputStream>>& files);

}  // namespace org::apache::nifi::minifi::c2
