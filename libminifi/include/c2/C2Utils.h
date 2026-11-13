// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
std::expected<std::shared_ptr<io::BufferStream>, std::string> createDebugBundleArchive(const std::map<std::string, std::unique_ptr<io::InputStream>>& files);

}  // namespace org::apache::nifi::minifi::c2
