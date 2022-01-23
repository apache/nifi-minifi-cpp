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

#include <string>
#include <memory>

#include "GCSProcessor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/retry_policy.h"

namespace org::apache::nifi::minifi::extensions::gcp {

class ListGCSBucket : public GCSProcessor {
 public:
  explicit ListGCSBucket(const std::string& name, const utils::Identifier& uuid = {})
      : GCSProcessor(name, uuid, core::logging::LoggerFactory<ListGCSBucket>::getLogger()) {
  }
  ListGCSBucket(const ListGCSBucket&) = delete;
  ListGCSBucket(ListGCSBucket&&) = delete;
  ListGCSBucket& operator=(const ListGCSBucket&) = delete;
  ListGCSBucket& operator=(ListGCSBucket&&) = delete;
  ~ListGCSBucket() override = default;

  EXTENSIONAPI static const core::Property Bucket;
  EXTENSIONAPI static const core::Property UseVersions;

  EXTENSIONAPI static const core::Relationship Success;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  bool isSingleThreaded() const override {
    return true;
  }

 private:
  std::string bucket_;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
