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

#include "minifi-cpp/properties/Locations.h"
#include "Defaults.h"
#include "fmt/format.h"

namespace org::apache::nifi::minifi {
class LocationsImpl final : public Locations {
  struct M {
    std::filesystem::path working_dir_;
    std::filesystem::path lock_path_;
    std::filesystem::path log_properties_path_;
    std::filesystem::path uid_properties_path_;
    std::filesystem::path properties_path_;
    std::filesystem::path logs_dir_;
    std::filesystem::path fips_bin_path_;
    std::filesystem::path fips_conf_path_;
    std::string extensions_pattern_;
  } m;

  explicit LocationsImpl(M m) : m(std::move(m)) {}

 public:
  static std::shared_ptr<LocationsImpl> createFromMinifiHome(const std::filesystem::path& minifi_home) {
    return std::shared_ptr<LocationsImpl>(new LocationsImpl(M{
      .working_dir_ = minifi_home,
      .lock_path_ = minifi_home / "LOCK",
      .log_properties_path_ = minifi_home / DEFAULT_LOG_PROPERTIES_FILE,
      .uid_properties_path_ = minifi_home / DEFAULT_UID_PROPERTIES_FILE,
      .properties_path_ = minifi_home / DEFAULT_NIFI_PROPERTIES_FILE,
      .logs_dir_ = minifi_home / "logs",
      .fips_bin_path_ = minifi_home / "fips",
      .fips_conf_path_ = minifi_home / "fips",
      .extensions_pattern_ = "../extensions/*"
    }));
  }

  static std::shared_ptr<LocationsImpl> createForFHS() {
    return std::shared_ptr<LocationsImpl>(new LocationsImpl(M{
        .working_dir_ = "/var/lib/nifi-minifi-cpp",
        .lock_path_ = "/var/lib/nifi-minifi-cpp/LOCK",
        .log_properties_path_ = "/etc/nifi-minifi-cpp/minifi-log.properties",
        .uid_properties_path_ = "/etc/nifi-minifi-cpp/minifi-uid.properties",
        .properties_path_ = "/etc/nifi-minifi-cpp/minifi.properties",
        .logs_dir_ = "/var/log/nifi-minifi-cpp",
        .fips_bin_path_ = "/usr/lib64/nifi-minifi-cpp/fips",
        .fips_conf_path_ = "/etc/nifi-minifi-cpp/fips",
        .extensions_pattern_ = "/usr/lib64/nifi-minifi-cpp/extensions/*"
    }));
  }

  [[nodiscard]] const std::filesystem::path& getWorkingDir() const override { return m.working_dir_; }
  [[nodiscard]] const std::filesystem::path& getLockPath() const override { return m.lock_path_; }
  [[nodiscard]] const std::filesystem::path& getLogPropertiesPath() const override { return m.log_properties_path_; }
  [[nodiscard]] const std::filesystem::path& getUidPropertiesPath() const override { return m.uid_properties_path_; }
  [[nodiscard]] const std::filesystem::path& getPropertiesPath() const override { return m.properties_path_; }
  [[nodiscard]] const std::filesystem::path& getFipsBinPath() const override { return m.fips_bin_path_; }
  [[nodiscard]] const std::filesystem::path& getFipsConfPath() const override { return m.fips_conf_path_; }
  [[nodiscard]] const std::filesystem::path& getLogsDirs() const override { return m.logs_dir_; }
  [[nodiscard]] std::string_view getDefaultExtensionsPattern() const override { return m.extensions_pattern_; }

  [[nodiscard]] std::string toString() const override {
    return fmt::format(
      R"(
Locations {{
  working dir: "{}",
  lock path: "{}",
  log properties path: "{}",
  uid properties path: "{}",
  properties path: "{}",
  fips bin path: "{}",
  fips conf path: "{}",
  logs dir: "{}",
  extensions pattern: "{}"
}})",
      getWorkingDir().string(),
      getLockPath().string(),
      getLogPropertiesPath().string(),
      getUidPropertiesPath().string(),
      getPropertiesPath().string(),
      getFipsBinPath().string(),
      getFipsConfPath().string(),
      getLogsDirs().string(),
      getDefaultExtensionsPattern());
  }
};
}  // namespace org::apache::nifi::minifi
