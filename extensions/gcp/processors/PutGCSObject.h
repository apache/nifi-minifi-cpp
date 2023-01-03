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

  explicit PutGCSObject(std::string name, const utils::Identifier& uuid = {})
      : GCSProcessor(std::move(name), uuid, core::logging::LoggerFactory<PutGCSObject>::getLogger(uuid)) {
  }
  ~PutGCSObject() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Puts flow files to a Google Cloud Storage Bucket.";

  EXTENSIONAPI static const core::Property Bucket;
  EXTENSIONAPI static const core::Property Key;
  EXTENSIONAPI static const core::Property ContentType;
  EXTENSIONAPI static const core::Property MD5Hash;
  EXTENSIONAPI static const core::Property Crc32cChecksum;
  EXTENSIONAPI static const core::Property EncryptionKey;
  EXTENSIONAPI static const core::Property ObjectACL;
  EXTENSIONAPI static const core::Property OverwriteObject;
  static auto properties() {
    return utils::array_cat(GCSProcessor::properties(), std::array{
      Bucket,
      Key,
      ContentType,
      MD5Hash,
      Crc32cChecksum,
      EncryptionKey,
      ObjectACL,
      OverwriteObject
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static const core::OutputAttribute Message;
  EXTENSIONAPI static const core::OutputAttribute Reason;
  EXTENSIONAPI static const core::OutputAttribute Domain;
  EXTENSIONAPI static const core::OutputAttribute BucketOutputAttribute;
  EXTENSIONAPI static const core::OutputAttribute KeyOutputAttribute;
  EXTENSIONAPI static const core::OutputAttribute Size;
  EXTENSIONAPI static const core::OutputAttribute Crc32c;
  EXTENSIONAPI static const core::OutputAttribute Md5;
  EXTENSIONAPI static const core::OutputAttribute OwnerEntity;
  EXTENSIONAPI static const core::OutputAttribute OwnerEntityId;
  EXTENSIONAPI static const core::OutputAttribute ContentEncoding;
  EXTENSIONAPI static const core::OutputAttribute ContentLanguage;
  EXTENSIONAPI static const core::OutputAttribute ContentDisposition;
  EXTENSIONAPI static const core::OutputAttribute MediaLink;
  EXTENSIONAPI static const core::OutputAttribute SelfLink;
  EXTENSIONAPI static const core::OutputAttribute Etag;
  EXTENSIONAPI static const core::OutputAttribute GeneratedId;
  EXTENSIONAPI static const core::OutputAttribute Generation;
  EXTENSIONAPI static const core::OutputAttribute Metageneration;
  EXTENSIONAPI static const core::OutputAttribute CreateTime;
  EXTENSIONAPI static const core::OutputAttribute UpdateTime;
  EXTENSIONAPI static const core::OutputAttribute DeleteTime;
  EXTENSIONAPI static const core::OutputAttribute EncryptionAlgorithm;
  EXTENSIONAPI static const core::OutputAttribute EncryptionSha256;
  static auto outputAttributes() {
    return std::array{
        Message,
        Reason,
        Domain,
        BucketOutputAttribute,
        KeyOutputAttribute,
        Size,
        Crc32c,
        Md5,
        OwnerEntity,
        OwnerEntityId,
        ContentEncoding,
        ContentLanguage,
        ContentDisposition,
        MediaLink,
        SelfLink,
        Etag,
        GeneratedId,
        Generation,
        Metageneration,
        CreateTime,
        UpdateTime,
        DeleteTime,
        EncryptionAlgorithm,
        EncryptionSha256
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

 private:
  google::cloud::storage::EncryptionKey encryption_key_;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
