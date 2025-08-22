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

#include <filesystem>
#include <string_view>

const inline std::filesystem::path DEFAULT_NIFI_CONFIG_YML = std::filesystem::path("conf") / "config.yml";
const inline std::filesystem::path DEFAULT_NIFI_CONFIG_JSON = std::filesystem::path("conf") / "config.json";
const inline std::filesystem::path DEFAULT_NIFI_PROPERTIES_FILE = std::filesystem::path("conf") / "minifi.properties";
const inline std::filesystem::path DEFAULT_LOG_PROPERTIES_FILE = std::filesystem::path("conf") / "minifi-log.properties";
const inline std::filesystem::path DEFAULT_UID_PROPERTIES_FILE = std::filesystem::path("conf") / "minifi-uid.properties";
const inline std::filesystem::path DEFAULT_BOOTSTRAP_FILE = std::filesystem::path("conf") / "bootstrap.conf";

constexpr inline std::string_view MINIFI_HOME_ENV_KEY = "MINIFI_HOME";
constexpr inline std::string_view MINIFI_HOME_ENV_VALUE_FHS = "FHS";
