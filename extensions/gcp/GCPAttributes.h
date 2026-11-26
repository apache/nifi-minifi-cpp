// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string_view>

#include "api/core/FlowFile.h"
#include "api/core/TriggerContext.h"
#include "google/cloud/storage/object_metadata.h"

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

inline void setAttributesFromObjectMetadata(api::core::FlowFile& flow_file, const ::google::cloud::storage::ObjectMetadata& object_metadata, api::core::TriggerContext& context) {
  context.setAttribute(flow_file, GCS_BUCKET_ATTR, object_metadata.bucket());
  context.setAttribute(flow_file, GCS_OBJECT_NAME_ATTR, object_metadata.name());
  context.setAttribute(flow_file, GCS_SIZE_ATTR, std::to_string(object_metadata.size()));
  context.setAttribute(flow_file, GCS_CRC32C_ATTR, object_metadata.crc32c());
  context.setAttribute(flow_file, GCS_MD5_ATTR, object_metadata.md5_hash());
  context.setAttribute(flow_file, GCS_CONTENT_ENCODING_ATTR, object_metadata.content_encoding());
  context.setAttribute(flow_file, GCS_CONTENT_LANGUAGE_ATTR, object_metadata.content_language());
  context.setAttribute(flow_file, GCS_CONTENT_DISPOSITION_ATTR, object_metadata.content_disposition());
  context.setAttribute(flow_file, GCS_CREATE_TIME_ATTR, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(object_metadata.time_created().time_since_epoch()).count()));
  context.setAttribute(flow_file, GCS_UPDATE_TIME_ATTR, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(object_metadata.updated().time_since_epoch()).count()));
  context.setAttribute(flow_file, GCS_DELETE_TIME_ATTR, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(object_metadata.time_deleted().time_since_epoch()).count()));
  context.setAttribute(flow_file, GCS_MEDIA_LINK_ATTR, object_metadata.media_link());
  context.setAttribute(flow_file, GCS_SELF_LINK_ATTR, object_metadata.self_link());
  context.setAttribute(flow_file, GCS_ETAG_ATTR, object_metadata.etag());
  context.setAttribute(flow_file, GCS_GENERATED_ID, object_metadata.id());
  context.setAttribute(flow_file, GCS_META_GENERATION, std::to_string(object_metadata.metageneration()));
  context.setAttribute(flow_file, GCS_GENERATION, std::to_string(object_metadata.generation()));
  context.setAttribute(flow_file, GCS_STORAGE_CLASS, object_metadata.storage_class());
  if (object_metadata.has_customer_encryption()) {
    context.setAttribute(flow_file, GCS_ENCRYPTION_ALGORITHM_ATTR, object_metadata.customer_encryption().encryption_algorithm);
    context.setAttribute(flow_file, GCS_ENCRYPTION_SHA256_ATTR, object_metadata.customer_encryption().key_sha256);
  }
  if (object_metadata.has_owner()) {
    context.setAttribute(flow_file, GCS_OWNER_ENTITY_ATTR, object_metadata.owner().entity);
    context.setAttribute(flow_file, GCS_OWNER_ENTITY_ID_ATTR, object_metadata.owner().entity_id);
  }
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
