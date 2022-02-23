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

#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "../controllerservices/GcpCredentialsControllerService.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/retry_policy.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::extensions::gcp {

class PutGcsObject : public core::Processor {
 public:
  SMART_ENUM(PredefinedAcl,
             (AUTHENTICATED_READ, "authenticatedRead"),
             (BUCKET_OWNER_FULL_CONTROL, "bucketOwnerFullControl"),
             (BUCKET_OWNER_READ_ONLY, "bucketOwnerRead"),
             (PRIVATE, "private"),
             (PROJECT_PRIVATE, "projectPrivate"),
             (PUBLIC_READ_ONLY, "publicRead"),
             (PUBLIC_READ_WRITE, "publicReadWrite"));

  explicit PutGcsObject(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }
  PutGcsObject(const PutGcsObject&) = delete;
  PutGcsObject(PutGcsObject&&) = delete;
  PutGcsObject& operator=(const PutGcsObject&) = delete;
  PutGcsObject& operator=(PutGcsObject&&) = delete;
  ~PutGcsObject() override = default;

  EXTENSIONAPI static const core::Property GCPCredentials;
  EXTENSIONAPI static const core::Property Bucket;
  EXTENSIONAPI static const core::Property ObjectName;
  EXTENSIONAPI static const core::Property NumberOfRetries;
  EXTENSIONAPI static const core::Property ContentType;
  EXTENSIONAPI static const core::Property MD5HashLocation;
  EXTENSIONAPI static const core::Property Crc32cChecksumLocation;
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

 protected:
  virtual google::cloud::storage::Client getClient(const google::cloud::storage::ClientOptions& options) const;

  google::cloud::storage::RetryPolicyOption::Type retry_policy_ = std::make_shared<google::cloud::storage::LimitedErrorCountRetryPolicy>(6);
  google::cloud::storage::EncryptionKey encryption_key_;

  std::shared_ptr<google::cloud::storage::oauth2::Credentials> gcp_credentials_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutGcsObject>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
