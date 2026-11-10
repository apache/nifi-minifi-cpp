// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
constexpr inline std::string_view MINIFI_INSTALLATION_TYPE_ENV_KEY = "MINIFI_INSTALLATION_TYPE";
constexpr inline std::string_view MINIFI_INSTALLATION_TYPE_FHS = "FHS";
