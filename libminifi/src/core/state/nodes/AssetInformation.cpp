// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/state/nodes/AssetInformation.h"
#include "core/Resource.h"
#include "core/logging/LoggerFactory.h"
#include "core/state/Value.h"

namespace org::apache::nifi::minifi::state::response {

AssetInformation::AssetInformation()
  : logger_(core::logging::LoggerFactory<AssetInformation>().getLogger()) {}

void AssetInformation::setAssetManager(utils::file::AssetManager* asset_manager) {
  asset_manager_ = asset_manager;
  if (!asset_manager_) {
    logger_->log_error("No asset manager is provided, asset information will not be available");
  }
}

std::vector<SerializedResponseNode> AssetInformation::serialize() {
  if (!asset_manager_) {
    return {};
  }
  SerializedResponseNode node;
  node.name = "hash";
  node.value = asset_manager_->hash();

  return std::vector<SerializedResponseNode>{node};
}

REGISTER_RESOURCE(AssetInformation, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
