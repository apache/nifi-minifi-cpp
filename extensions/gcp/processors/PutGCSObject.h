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
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"
#include "google/cloud/storage/well_known_headers.h"

namespace org::apache::nifi::minifi::extensions::gcp {

class PutGCSObject : public GCSProcessor {
 public:
  SMART_ENUM(PredefinedAcl,
             (AUTHENTICATED_READ, "authenticatedRead"),
             (BUCKET_OWNER_FULL_CONTROL, "bucketOwnerFullControl"),
             (BUCKET_OWNER_READ_ONLY, "bucketOwnerRead"),
             (PRIVATE, "private"),
             (PROJECT_PRIVATE, "projectPrivate"),
             (PUBLIC_READ_ONLY, "publicRead"),
             (PUBLIC_READ_WRITE, "publicReadWrite"));

  explicit PutGCSObject(const std::string& name, const utils::Identifier& uuid = {})
      : GCSProcessor(name, uuid, core::logging::LoggerFactory<PutGCSObject>::getLogger()) {
  }
  ~PutGCSObject() override = default;

  EXTENSIONAPI static const core::Property Bucket;
  EXTENSIONAPI static const core::Property Key;
  EXTENSIONAPI static const core::Property ContentType;
  EXTENSIONAPI static const core::Property MD5Hash;
  EXTENSIONAPI static const core::Property Crc32cChecksum;
  EXTENSIONAPI static const core::Property EncryptionKey;
  EXTENSIONAPI static const core::Property ObjectACL;
  EXTENSIONAPI static const core::Property OverwriteObject;

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  bool isSingleThreaded() const override {
    return true;
  }

 private:
  google::cloud::storage::EncryptionKey encryption_key_;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
