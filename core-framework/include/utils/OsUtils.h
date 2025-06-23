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

#include <cstdint>
#include <string>
#include <optional>
#include <system_error>

struct sockaddr;

namespace org::apache::nifi::minifi::utils::OsUtils {

/// Resolves a user ID to a username
extern std::string userIdToUsername(const std::string &uid);

/// Returns physical memory usage by the current process in bytes
int64_t getCurrentProcessPhysicalMemoryUsage();

int64_t getCurrentProcessId();

/// Returns physical memory usage by the system in bytes
int64_t getSystemPhysicalMemoryUsage();

/// Returns the total physical memory in the system in bytes
int64_t getSystemTotalPhysicalMemory();

#ifdef WIN32
/// Returns the total paging file size in bytes
int64_t getTotalPagingFileSize();

std::error_code windowsErrorToErrorCode(unsigned long error_code);  // NOLINT [runtime/int]
#endif

/// Returns the host architecture (e.g. x32, arm64)
std::string getMachineArchitecture();

#ifdef WIN32
/// Resolves common identifiers
extern std::string resolve_common_identifiers(const std::string &id);
#endif

std::optional<std::string> getHostName();
std::optional<double> getSystemLoadAverage();

}  // namespace org::apache::nifi::minifi::utils::OsUtils
