// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "core/state/nodes/MetricsBase.h"
#include "utils/file/AssetManager.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::state::response {

class AssetInformation : public ResponseNodeImpl {
 public:
  AssetInformation();
  explicit AssetInformation(std::string_view name, const utils::Identifier& uuid = {}) : ResponseNodeImpl(name, uuid) {}

  MINIFIAPI static constexpr const char* Description = "Metric node that defines hash for all asset identifiers";

  void setAssetManager(utils::file::AssetManager* asset_manager);

  std::string getName() const override { return "resourceInfo"; }
  std::vector<SerializedResponseNode> serialize() override;

 private:
  utils::file::AssetManager* asset_manager_{nullptr};
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::state::response
