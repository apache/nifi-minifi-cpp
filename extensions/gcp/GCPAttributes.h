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

#include <string_view>

#include "google/cloud/storage/object_metadata.h"
#include "minifi-cpp/core/FlowFile.h"

namespace org::apache::nifi::minifi::extensions::gcp {

constexpr std::string_view GCS_ERROR_REASON = "gcs.error.reason";
constexpr std::string_view GCS_ERROR_DOMAIN = "gcs.error.domain";
constexpr std::string_view GCS_STATUS_MESSAGE = "gcs.status.message";
constexpr std::string_view GCS_BUCKET_ATTR = "gcs.bucket";
constexpr std::string_view GCS_OBJECT_NAME_ATTR = "gcs.key";
constexpr std::string_view GCS_SIZE_ATTR = "gcs.size";
constexpr std::string_view GCS_CRC32C_ATTR = "gcs.crc32c";
constexpr std::string_view GCS_MD5_ATTR = "gcs.md5";
constexpr std::string_view GCS_OWNER_ENTITY_ATTR = "gcs.owner.entity";
constexpr std::string_view GCS_OWNER_ENTITY_ID_ATTR = "gcs.owner.entity.id";
constexpr std::string_view GCS_MEDIA_LINK_ATTR = "gcs.media.link";
constexpr std::string_view GCS_ETAG_ATTR = "gcs.etag";
constexpr std::string_view GCS_GENERATED_ID = "gcs.generated.id";
constexpr std::string_view GCS_GENERATION = "gcs.generation";
constexpr std::string_view GCS_META_GENERATION = "gcs.metageneration";
constexpr std::string_view GCS_STORAGE_CLASS = "gcs.storage.class";
constexpr std::string_view GCS_CONTENT_ENCODING_ATTR = "gcs.content.encoding";
constexpr std::string_view GCS_CONTENT_LANGUAGE_ATTR = "gcs.content.language";
constexpr std::string_view GCS_CONTENT_DISPOSITION_ATTR = "gcs.content.disposition";
constexpr std::string_view GCS_CREATE_TIME_ATTR = "gcs.create.time";
constexpr std::string_view GCS_DELETE_TIME_ATTR = "gcs.delete.time";
constexpr std::string_view GCS_UPDATE_TIME_ATTR = "gcs.update.time";
constexpr std::string_view GCS_SELF_LINK_ATTR = "gcs.self.link";
constexpr std::string_view GCS_ENCRYPTION_ALGORITHM_ATTR = "gcs.encryption.algorithm";
constexpr std::string_view GCS_ENCRYPTION_SHA256_ATTR = "gcs.encryption.sha256";

inline void setAttributesFromObjectMetadata(core::FlowFile& flow_file, const ::google::cloud::storage::ObjectMetadata& object_metadata) {
  flow_file.setAttribute(GCS_BUCKET_ATTR, object_metadata.bucket());
  flow_file.setAttribute(GCS_OBJECT_NAME_ATTR, object_metadata.name());
  flow_file.setAttribute(GCS_SIZE_ATTR, std::to_string(object_metadata.size()));
  flow_file.setAttribute(GCS_CRC32C_ATTR, object_metadata.crc32c());
  flow_file.setAttribute(GCS_MD5_ATTR, object_metadata.md5_hash());
  flow_file.setAttribute(GCS_CONTENT_ENCODING_ATTR, object_metadata.content_encoding());
  flow_file.setAttribute(GCS_CONTENT_LANGUAGE_ATTR, object_metadata.content_language());
  flow_file.setAttribute(GCS_CONTENT_DISPOSITION_ATTR, object_metadata.content_disposition());
  flow_file.setAttribute(GCS_CREATE_TIME_ATTR, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(object_metadata.time_created().time_since_epoch()).count()));
  flow_file.setAttribute(GCS_UPDATE_TIME_ATTR, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(object_metadata.updated().time_since_epoch()).count()));
  flow_file.setAttribute(GCS_DELETE_TIME_ATTR, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(object_metadata.time_deleted().time_since_epoch()).count()));
  flow_file.setAttribute(GCS_MEDIA_LINK_ATTR, object_metadata.media_link());
  flow_file.setAttribute(GCS_SELF_LINK_ATTR, object_metadata.self_link());
  flow_file.setAttribute(GCS_ETAG_ATTR, object_metadata.etag());
  flow_file.setAttribute(GCS_GENERATED_ID, object_metadata.id());
  flow_file.setAttribute(GCS_META_GENERATION, std::to_string(object_metadata.metageneration()));
  flow_file.setAttribute(GCS_GENERATION, std::to_string(object_metadata.generation()));
  flow_file.setAttribute(GCS_STORAGE_CLASS, object_metadata.storage_class());
  if (object_metadata.has_customer_encryption()) {
    flow_file.setAttribute(GCS_ENCRYPTION_ALGORITHM_ATTR, object_metadata.customer_encryption().encryption_algorithm);
    flow_file.setAttribute(GCS_ENCRYPTION_SHA256_ATTR, object_metadata.customer_encryption().key_sha256);
  }
  if (object_metadata.has_owner()) {
    flow_file.setAttribute(GCS_OWNER_ENTITY_ATTR, object_metadata.owner().entity);
    flow_file.setAttribute(GCS_OWNER_ENTITY_ID_ATTR, object_metadata.owner().entity_id);
  }
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
