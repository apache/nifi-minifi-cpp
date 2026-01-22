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
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <set>
#include "minifi-cpp/core/logging/Logger.h"
#include "utils/expected.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::utils::file {

struct AssetDescription {
  std::string id;
  std::filesystem::path path;
  std::string url;

  bool operator<(const AssetDescription& other) const {
    return id < other.id;
  }
};

struct AssetLayout {
  std::string digest;
  std::set<AssetDescription> assets;

  void clear() {
    digest.clear();
    assets.clear();
  }
};

class AssetManager {
 public:
  explicit AssetManager(const Configure& configuration);

  nonstd::expected<void, std::string> sync(const AssetLayout& layout, const std::function<nonstd::expected<void, std::string>(std::string_view /*url*/, const std::filesystem::path& /*tmp_path*/)>& fetch);

  std::string hash() const;

  std::filesystem::path getRoot() const;

  std::optional<std::filesystem::path> findAssetById(std::string_view id) const;

 private:
  void refreshState();

  void persist() const;

  mutable std::recursive_mutex mtx_;
  std::filesystem::path root_;
  AssetLayout state_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::utils::file
