// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

  std::expected<void, std::string> sync(const AssetLayout& layout,
      const std::function<std::expected<void, std::string>(std::string_view /*url*/, const std::filesystem::path& /*tmp_path*/)>& fetch);

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
