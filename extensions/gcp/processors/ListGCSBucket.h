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

class ListGCSBucket : public GCSProcessor {
 public:
  explicit ListGCSBucket(std::string name, const utils::Identifier& uuid = {})
      : GCSProcessor(std::move(name), uuid, core::logging::LoggerFactory<ListGCSBucket>::getLogger()) {
  }
  ~ListGCSBucket() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Retrieves a listing of objects from an GCS bucket. "
      "For each object that is listed, creates a FlowFile that represents the object so that it can be fetched in conjunction with FetchGCSObject.";

  EXTENSIONAPI static const core::Property Bucket;
  EXTENSIONAPI static const core::Property ListAllVersions;
  static auto properties() {
    return utils::array_cat(GCSProcessor::properties(), std::array{
      Bucket,
      ListAllVersions
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

 private:
  std::string bucket_;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
