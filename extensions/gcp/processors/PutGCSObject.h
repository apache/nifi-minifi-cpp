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

#include "../GCPAttributes.h"
#include "GCSProcessor.h"
#include "PropertyDefinition.h"
#include "PropertyType.h"
#include "RelationshipDefinition.h"
#include "core/logging/LoggerConfiguration.h"
#include "RelationshipDefinition.h"
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

  EXTENSIONAPI static constexpr auto Bucket = core::PropertyDefinitionBuilder<>::createProperty("Bucket")
      .withDescription("Bucket of the object.")
      .withDefaultValue("${gcs.bucket}")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Key = core::PropertyDefinitionBuilder<>::createProperty("Key")
      .withDescription("Name of the object.")
      .withDefaultValue("${filename}")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ContentType = core::PropertyDefinitionBuilder<>::createProperty("Content Type")
      .withDescription("Content Type for the file, i.e. text/plain ")
      .isRequired(false)
      .withDefaultValue("${mime.type}")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto MD5Hash = core::PropertyDefinitionBuilder<>::createProperty("MD5 Hash")
      .withDescription("MD5 Hash (encoded in Base64) of the file for server-side validation.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Crc32cChecksum = core::PropertyDefinitionBuilder<>::createProperty("CRC32C Checksum")
      .withDescription("CRC32C Checksum (encoded in Base64, big-Endian order) of the file for server-side validation.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto EncryptionKey = core::PropertyDefinitionBuilder<>::createProperty("Server Side Encryption Key")
      .withDescription("An AES256 Encryption Key (encoded in base64) for server-side encryption of the object.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ObjectACL = core::PropertyDefinitionBuilder<PredefinedAcl::length>::createProperty("Object ACL")
      .withDescription("Access Control to be attached to the object uploaded. Not providing this will revert to bucket defaults.")
      .isRequired(false)
      .withAllowedValues(PredefinedAcl::values)
      .build();
  EXTENSIONAPI static constexpr auto OverwriteObject = core::PropertyDefinitionBuilder<>::createProperty("Overwrite Object")
      .withDescription("If false, the upload to GCS will succeed only if the object does not exist.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(GCSProcessor::Properties, std::array<core::PropertyReference, 8>{
      Bucket,
      Key,
      ContentType,
      MD5Hash,
      Crc32cChecksum,
      EncryptionKey,
      ObjectACL,
      OverwriteObject
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Files that have been successfully written to Google Cloud Storage are transferred to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Files that could not be written to Google Cloud Storage for some reason are transferred to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto Message = core::OutputAttributeDefinition<>{GCS_STATUS_MESSAGE, { Failure }, "The status message received from google cloud."};
  EXTENSIONAPI static constexpr auto Reason = core::OutputAttributeDefinition<>{GCS_ERROR_REASON, { Failure }, "The description of the error occurred during upload."};
  EXTENSIONAPI static constexpr auto Domain = core::OutputAttributeDefinition<>{GCS_ERROR_DOMAIN, { Failure }, "The domain of the error occurred during upload."};
  EXTENSIONAPI static constexpr auto BucketOutputAttribute = core::OutputAttributeDefinition<>{GCS_BUCKET_ATTR, { Success }, "Bucket of the object."};
  EXTENSIONAPI static constexpr auto KeyOutputAttribute = core::OutputAttributeDefinition<>{GCS_OBJECT_NAME_ATTR, { Success }, "Name of the object."};
  EXTENSIONAPI static constexpr auto Size = core::OutputAttributeDefinition<>{GCS_SIZE_ATTR, { Success }, "Size of the object."};
  EXTENSIONAPI static constexpr auto Crc32c = core::OutputAttributeDefinition<>{GCS_CRC32C_ATTR, { Success }, "The CRC32C checksum of object's data, encoded in base64."};
  EXTENSIONAPI static constexpr auto Md5 = core::OutputAttributeDefinition<>{GCS_MD5_ATTR, { Success }, "The MD5 hash of the object's data, encoded in base64."};
  EXTENSIONAPI static constexpr auto OwnerEntity = core::OutputAttributeDefinition<>{GCS_OWNER_ENTITY_ATTR, { Success }, "The owner entity, in the form \"user-emailAddress\"."};
  EXTENSIONAPI static constexpr auto OwnerEntityId = core::OutputAttributeDefinition<>{GCS_OWNER_ENTITY_ID_ATTR, { Success }, "The ID for the entity."};
  EXTENSIONAPI static constexpr auto ContentEncoding = core::OutputAttributeDefinition<>{GCS_CONTENT_ENCODING_ATTR, { Success }, "The content encoding of the object."};
  EXTENSIONAPI static constexpr auto ContentLanguage = core::OutputAttributeDefinition<>{GCS_CONTENT_LANGUAGE_ATTR, { Success }, "The content language of the object."};
  EXTENSIONAPI static constexpr auto ContentDisposition = core::OutputAttributeDefinition<>{GCS_CONTENT_DISPOSITION_ATTR, { Success }, "The data content disposition of the object."};
  EXTENSIONAPI static constexpr auto MediaLink = core::OutputAttributeDefinition<>{GCS_MEDIA_LINK_ATTR, { Success }, "The media download link to the object."};
  EXTENSIONAPI static constexpr auto SelfLink = core::OutputAttributeDefinition<>{GCS_SELF_LINK_ATTR, { Success }, "The link to this object."};
  EXTENSIONAPI static constexpr auto Etag = core::OutputAttributeDefinition<>{GCS_ETAG_ATTR, { Success }, "The HTTP 1.1 Entity tag for the object."};
  EXTENSIONAPI static constexpr auto GeneratedId = core::OutputAttributeDefinition<>{GCS_GENERATED_ID, { Success }, "The service-generated ID for the object."};
  EXTENSIONAPI static constexpr auto Generation = core::OutputAttributeDefinition<>{GCS_GENERATION, { Success }, "The content generation of this object. Used for object versioning."};
  EXTENSIONAPI static constexpr auto Metageneration = core::OutputAttributeDefinition<>{GCS_META_GENERATION, { Success }, "The metageneration of the object."};
  EXTENSIONAPI static constexpr auto CreateTime = core::OutputAttributeDefinition<>{GCS_CREATE_TIME_ATTR, { Success }, "Unix timestamp of the object's creation in milliseconds."};
  EXTENSIONAPI static constexpr auto UpdateTime = core::OutputAttributeDefinition<>{GCS_UPDATE_TIME_ATTR, { Success }, "Unix timestamp of the object's last modification in milliseconds."};
  EXTENSIONAPI static constexpr auto DeleteTime = core::OutputAttributeDefinition<>{GCS_DELETE_TIME_ATTR, { Success }, "Unix timestamp of the object's deletion in milliseconds."};
  EXTENSIONAPI static constexpr auto EncryptionAlgorithm = core::OutputAttributeDefinition<>{GCS_ENCRYPTION_ALGORITHM_ATTR, { Success }, "The algorithm used to encrypt the object."};
  EXTENSIONAPI static constexpr auto EncryptionSha256 = core::OutputAttributeDefinition<>{GCS_ENCRYPTION_SHA256_ATTR, { Success }, "The SHA256 hash of the key used to encrypt the object."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 24>{
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
