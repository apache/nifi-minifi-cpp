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

#include <memory>
#include <string>
#include <utility>

#include "GCSProcessor.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::extensions::gcp {

class DeleteGCSObject : public GCSProcessor {
 public:
  explicit DeleteGCSObject(std::string name, const utils::Identifier& uuid = {})
      : GCSProcessor(std::move(name), uuid, core::logging::LoggerFactory<DeleteGCSObject>::getLogger()) {
  }
  ~DeleteGCSObject() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Deletes an object from a Google Cloud Bucket.";

  EXTENSIONAPI static const core::Property Bucket;
  EXTENSIONAPI static const core::Property Key;
  EXTENSIONAPI static const core::Property EncryptionKey;
  EXTENSIONAPI static const core::Property ObjectGeneration;
  static auto properties() {
    return utils::array_cat(GCSProcessor::properties(), std::array{
      Bucket,
      Key,
      EncryptionKey,
      ObjectGeneration
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
