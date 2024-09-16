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
