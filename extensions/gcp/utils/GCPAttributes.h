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

namespace org::apache::nifi::minifi::extensions::gcp {

constexpr const char* GCS_ERROR_REASON = "gcs.error.reason";
constexpr const char* GCS_ERROR_DOMAIN = "gcs.error.domain";
constexpr const char* GCS_BUCKET_ATTR = "gcs.bucket";
constexpr const char* GCS_OBJECT_NAME_ATTR = "gcs.key";
constexpr const char* GCS_SIZE_ATTR = "gcs.size";
constexpr const char* GCS_CRC32C_ATTR = "gcs.crc32c";
constexpr const char* GCS_MD5_ATTR = "gcs.md5";
constexpr const char* GCS_OWNER_ENTITY_ATTR = "gcs.owner.entity";
constexpr const char* GCS_OWNER_ENTITY_ID_ATTR = "gcs.owner.entity.id";
constexpr const char* GCS_MEDIA_LINK_ATTR = "gcs.media.link";
constexpr const char* GCS_ETAG_ATTR = "gcs.etag";
constexpr const char* GCS_GENERATED_ID = "gcs.generated.id";
constexpr const char* GCS_GENERATION = "gcs.generation";
constexpr const char* GCS_CONTENT_ENCODING_ATTR = "gcs.content.encoding";
constexpr const char* GCS_CONTENT_LANGUAGE_ATTR = "gcs.content.language";
constexpr const char* GCS_CONTENT_DISPOSITION_ATTR = "gcs.content.disposition";
constexpr const char* GCS_CREATE_TIME_ATTR = "gcs.create.time";
constexpr const char* GCS_DELETE_TIME_ATTR = "gcs.delete.time";
constexpr const char* GCS_UPDATE_TIME_ATTR = "gcs.update.time";
constexpr const char* GCS_SELF_LINK_ATTR = "gcs.self.link";
constexpr const char* GCS_ENCRYPTION_ALGORITHM_ATTR = "gcs.encryption.algorithm";
constexpr const char* GCS_ENCRYPTION_SHA256_ATTR = "gcs.encryption.sha256";

}  // namespace org::apache::nifi::minifi::extensions::gcp
