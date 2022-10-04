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

#include "PutGCSObject.h"

#include <utility>

#include "core/Resource.h"
#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "../GCPAttributes.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {
namespace {
class UploadToGCSCallback {
 public:
  UploadToGCSCallback(gcs::Client& client, std::string bucket, std::string key)
      : bucket_(std::move(bucket)),
        key_(std::move(key)),
        client_(client) {
  }

  int64_t operator()(const std::shared_ptr<io::InputStream>& stream) {
    std::string content;
    content.resize(stream->size());
    const auto read_ret = stream->read(gsl::make_span(content).as_span<std::byte>());
    if (io::isError(read_ret)) {
      return -1;
    }
    auto writer = client_.WriteObject(bucket_, key_, hash_value_, crc32c_checksum_, encryption_key_, content_type_, predefined_acl_, if_generation_match_);
    writer << content;
    writer.Close();
    result_ = writer.metadata();
    return read_ret;
  }

  [[nodiscard]] const google::cloud::StatusOr<gcs::ObjectMetadata>& getResult() const noexcept {
    return result_;
  }

  void setHashValue(const std::string& hash_value_str) {
    hash_value_ = gcs::MD5HashValue(hash_value_str);
  }

  void setCrc32CChecksumValue(const std::string& crc32c_checksum_str) {
    crc32c_checksum_ = gcs::Crc32cChecksumValue(crc32c_checksum_str);
  }

  void setEncryptionKey(const gcs::EncryptionKey& encryption_key) {
    encryption_key_ = encryption_key;
  }

  void setPredefinedAcl(PutGCSObject::PredefinedAcl predefined_acl) {
    predefined_acl_ = gcs::PredefinedAcl(predefined_acl.toString());
  }

  void setContentType(const std::string& content_type_str) {
    content_type_ = gcs::ContentType(content_type_str);
  }

  void setIfGenerationMatch(std::optional<bool> overwrite) {
    if (overwrite.has_value() && !overwrite.value()) {
      if_generation_match_ = gcs::IfGenerationMatch(0);
    } else {
      if_generation_match_ = gcs::IfGenerationMatch();
    }
  }

 private:
  std::string bucket_;
  std::string key_;
  gcs::Client& client_;

  gcs::MD5HashValue hash_value_;
  gcs::Crc32cChecksumValue crc32c_checksum_;
  gcs::EncryptionKey encryption_key_;
  gcs::PredefinedAcl predefined_acl_;
  gcs::ContentType content_type_;
  gcs::IfGenerationMatch if_generation_match_;

  google::cloud::StatusOr<gcs::ObjectMetadata> result_;
};

}  // namespace


void PutGCSObject::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}


void PutGCSObject::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& session_factory) {
  GCSProcessor::onSchedule(context, session_factory);
  gsl_Expects(context);
  if (auto encryption_key = context->getProperty(EncryptionKey)) {
    try {
      encryption_key_ = gcs::EncryptionKey::FromBase64Key(*encryption_key);
    } catch (const google::cloud::RuntimeStatusError&) {
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Could not decode the base64-encoded encryption key from property " + EncryptionKey.getName());
    }
  }
}

void PutGCSObject::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session && gcp_credentials_);

  auto flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  auto bucket = context->getProperty(Bucket, flow_file);
  if (!bucket || bucket->empty()) {
    logger_->log_error("Missing bucket name");
    session->transfer(flow_file, Failure);
    return;
  }
  auto object_name = context->getProperty(Key, flow_file);
  if (!object_name || object_name->empty()) {
    logger_->log_error("Missing object name");
    session->transfer(flow_file, Failure);
    return;
  }

  gcs::Client client = getClient();
  UploadToGCSCallback callback(client, *bucket, *object_name);

  if (auto crc32_checksum = context->getProperty(Crc32cChecksum, flow_file)) {
    callback.setCrc32CChecksumValue(*crc32_checksum);
  }

  if (auto md5_hash = context->getProperty(MD5Hash, flow_file)) {
    callback.setHashValue(*md5_hash);
  }

  auto content_type = context->getProperty(ContentType, flow_file);
  if (content_type && !content_type->empty())
    callback.setContentType(*content_type);

  if (auto predefined_acl = context->getProperty<PredefinedAcl>(ObjectACL))
    callback.setPredefinedAcl(*predefined_acl);
  callback.setIfGenerationMatch(context->getProperty<bool>(OverwriteObject));

  callback.setEncryptionKey(encryption_key_);

  session->read(flow_file, std::ref(callback));
  auto& result = callback.getResult();
  if (!result.ok()) {
    flow_file->setAttribute(GCS_STATUS_MESSAGE, result.status().message());
    flow_file->setAttribute(GCS_ERROR_REASON, result.status().error_info().reason());
    flow_file->setAttribute(GCS_ERROR_DOMAIN, result.status().error_info().domain());
    logger_->log_error("Failed to upload to Google Cloud Storage %s %s", result.status().message(), result.status().error_info().reason());
    session->transfer(flow_file, Failure);
  } else {
    setAttributesFromObjectMetadata(*flow_file, *result);
    session->transfer(flow_file, Success);
  }
}
}  // namespace org::apache::nifi::minifi::extensions::gcp
