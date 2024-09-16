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
#include "core/logging/LoggerFactory.h"
#include "core/OutputAttributeDefinition.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::extensions::gcp {

namespace detail {
inline constexpr std::string_view FILENAME_OUTPUT_ATTRIBUTE_DESCRIPTION_PART_1{"Same as "};
inline constexpr auto FILENAME_OUTPUT_ATTRIBUTE_DESCRIPTION_ARRAY = utils::array_cat(
    utils::string_view_to_array<FILENAME_OUTPUT_ATTRIBUTE_DESCRIPTION_PART_1.size()>(FILENAME_OUTPUT_ATTRIBUTE_DESCRIPTION_PART_1),
    utils::string_view_to_array<GCS_OBJECT_NAME_ATTR.size()>(GCS_OBJECT_NAME_ATTR));
inline constexpr auto FILENAME_OUTPUT_ATTRIBUTE_DESCRIPTION = utils::array_to_string_view(FILENAME_OUTPUT_ATTRIBUTE_DESCRIPTION_ARRAY);
}  // namespace detail

class ListGCSBucket : public GCSProcessor {
 public:
  explicit ListGCSBucket(std::string_view name, const utils::Identifier& uuid = {})
      : GCSProcessor(name, uuid, core::logging::LoggerFactory<ListGCSBucket>::getLogger(uuid)) {
  }
  ~ListGCSBucket() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Retrieves a listing of objects from an GCS bucket. "
      "For each object that is listed, creates a FlowFile that represents the object so that it can be fetched in conjunction with FetchGCSObject.";

  EXTENSIONAPI static constexpr auto Bucket = core::PropertyDefinitionBuilder<>::createProperty("Bucket")
      .withDescription("Bucket of the object.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ListAllVersions = core::PropertyDefinitionBuilder<>::createProperty("List all versions")
      .withDescription("Set this option to `true` to get all the previous versions separately.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(GCSProcessor::Properties, std::to_array<core::PropertyReference>({
      Bucket,
      ListAllVersions
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to this relationship after a successful Google Cloud Storage operation."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto BucketOutputAttribute = core::OutputAttributeDefinition<>{GCS_BUCKET_ATTR, { Success }, "Bucket of the object."};
  EXTENSIONAPI static constexpr auto Key = core::OutputAttributeDefinition<>{GCS_OBJECT_NAME_ATTR, { Success }, "Name of the object."};
  EXTENSIONAPI static constexpr auto Filename = core::OutputAttributeDefinition<>{"filename", { Success }, detail::FILENAME_OUTPUT_ATTRIBUTE_DESCRIPTION};
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
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 22>{
      BucketOutputAttribute,
      Key,
      Filename,
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
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::string bucket_;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
