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

#include "PutGcsObject.h"

#include <vector>
#include <utility>

#include "core/Resource.h"
#include "core/FlowFile.h"
#include "utils/OptionalUtils.h"
#include "../GCPAttributes.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {
const core::Property PutGcsObject::GCPCredentials(
    core::PropertyBuilder::createProperty("GCP Credentials Provider Service")
        ->withDescription("The Controller Service used to obtain Google Cloud Platform credentials.")
        ->isRequired(true)
        ->asType<GcpCredentialsControllerService>()
        ->build());

const core::Property PutGcsObject::Bucket(
    core::PropertyBuilder::createProperty("Bucket Name")
        ->withDescription("The name of the Bucket to upload to. If left empty the gcs.bucket attribute will be used by default.")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGcsObject::ObjectName(
    core::PropertyBuilder::createProperty("Object Name")
        ->withDescription("The name of the object to be uploaded. If left empty the filename attribute will be used by default.")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGcsObject::NumberOfRetries(
    core::PropertyBuilder::createProperty("Number of retries")
        ->withDescription("How many retry attempts should be made before routing to the failure relationship.")
        ->withDefaultValue<uint64_t>(6)
        ->isRequired(true)
        ->supportsExpressionLanguage(false)
        ->build());

const core::Property PutGcsObject::ContentType(
    core::PropertyBuilder::createProperty("Content Type")
        ->withDescription("The Content Type of the uploaded object. If not set, \"mime.type\" flow file attribute will be used. "
                          "In case of neither of them is specified, this information will not be sent to the server.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGcsObject::MD5HashLocation(
    core::PropertyBuilder::createProperty("MD5 Hash location")
        ->withDescription("The name of the attribute where the md5 hash is stored for server-side validation.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGcsObject::Crc32cChecksumLocation(
    core::PropertyBuilder::createProperty("CRC32 Checksum location")
        ->withDescription("The name of the attribute where the crc32 checksum is stored for server-side validation.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGcsObject::EncryptionKey(
    core::PropertyBuilder::createProperty("Server Side Encryption Key")
        ->withDescription("An AES256 Encryption Key (encoded in base64) for server-side encryption of the object.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGcsObject::ObjectACL(
    core::PropertyBuilder::createProperty("Object ACL")
        ->withDescription("Access Control to be attached to the object uploaded. Not providing this will revert to bucket defaults.")
        ->isRequired(false)
        ->withAllowableValues(PredefinedAcl::values())
        ->build());

const core::Property PutGcsObject::OverwriteObject(
    core::PropertyBuilder::createProperty("Overwrite Object")
        ->withDescription("If false, the upload to GCS will succeed only if the object does not exist.")
        ->withDefaultValue<bool>(true)
        ->build());

const core::Property PutGcsObject::EndpointOverrideURL(
    core::PropertyBuilder::createProperty("Endpoint Override URL")
        ->withDescription("Overrides the default Google Cloud Storage endpoints")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Relationship PutGcsObject::Success("success", "Files that have been successfully written to Google Cloud Storage are transferred to this relationship");
const core::Relationship PutGcsObject::Failure("failure", "Files that could not be written to Google Cloud Storage for some reason are transferred to this relationship");


namespace {
class UploadToGCSCallback : public InputStreamCallback {
 public:
  UploadToGCSCallback(gcs::Client& client, std::string bucket, std::string key)
      : bucket_(std::move(bucket)),
        key_(std::move(key)),
        client_(client) {
  }

  int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
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

  void setPredefinedAcl(PutGcsObject::PredefinedAcl predefined_acl) {
    predefined_acl_ = gcs::PredefinedAcl(predefined_acl.toString());
  }

  void setContentType(const std::string& content_type_str) {
    content_type_ = gcs::ContentType(content_type_str);
  }

  void setIfGenerationMatch(std::optional<bool> overwrite) {
    if (overwrite.has_value() && overwrite.value() == false) {
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

[[nodiscard]] std::optional<std::string> getContentType(const core::ProcessContext& context, const core::FlowFile& flow_file) {
  return context.getProperty(PutGcsObject::ContentType) | utils::orElse([&flow_file] {return flow_file.getAttribute("mime.type");});
}

std::shared_ptr<google::cloud::storage::oauth2::Credentials> getCredentials(core::ProcessContext& context) {
  std::string service_name;
  if (context.getProperty(PutGcsObject::GCPCredentials.getName(), service_name) && !IsNullOrEmpty(service_name))
    return std::dynamic_pointer_cast<const GcpCredentialsControllerService>(context.getControllerService(service_name))->getCredentials();
  return nullptr;
}
}  // namespace


void PutGcsObject::initialize() {
  setSupportedProperties({GCPCredentials,
                          Bucket,
                          ObjectName,
                          NumberOfRetries,
                          ContentType,
                          MD5HashLocation,
                          Crc32cChecksumLocation,
                          EncryptionKey,
                          ObjectACL,
                          OverwriteObject,
                          EndpointOverrideURL});
  setSupportedRelationships({Success, Failure});
}

gcs::Client PutGcsObject::getClient(const gcs::ClientOptions& options) const {
  return gcs::Client(options, *retry_policy_);
}


void PutGcsObject::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);
  if (auto number_of_retries = context->getProperty<uint64_t>(NumberOfRetries)) {
    retry_policy_ = std::make_shared<google::cloud::storage::LimitedErrorCountRetryPolicy>(*number_of_retries);
  }
  if (auto encryption_key = context->getProperty(EncryptionKey)) {
    try {
      encryption_key_ = gcs::EncryptionKey::FromBase64Key(*encryption_key);
    } catch (const google::cloud::RuntimeStatusError&) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, EncryptionKey.getName() + " is not in base64: " + *encryption_key);
    }
  }
  gcp_credentials_ = getCredentials(*context);
}

void PutGcsObject::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session);

  if (!gcp_credentials_) {
    logger_->log_error("Invalid or missing credentials from Google Cloud Platform Credentials Controller Service");
    context->yield();
    return;
  }

  auto flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  auto bucket = context->getProperty(Bucket, flow_file) | utils::orElse([&flow_file] {return flow_file->getAttribute(GCS_BUCKET_ATTR);});
  if (!bucket) {
    logger_->log_error("Missing bucket name");
    session->transfer(flow_file, Failure);
    return;
  }
  auto object_name = context->getProperty(ObjectName, flow_file) | utils::orElse([&flow_file] {return flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME);});
  if (!object_name) {
    logger_->log_error("Missing object name");
    session->transfer(flow_file, Failure);
    return;
  }

  auto options = gcs::ClientOptions(gcp_credentials_);
  if (auto endpoint_override_url = context->getProperty(EndpointOverrideURL)) {
    options.set_endpoint(*endpoint_override_url);
    logger_->log_debug("Endpoint override url %s", *endpoint_override_url);
  }

  gcs::Client client = getClient(options);
  UploadToGCSCallback callback(client, *bucket, *object_name);

  if (auto crc32_checksum_location = context->getProperty(Crc32cChecksumLocation, flow_file)) {
    if (auto crc32_checksum = flow_file->getAttribute(*crc32_checksum_location)) {
      callback.setCrc32CChecksumValue(*crc32_checksum);
    }
  }

  if (auto md5_hash_location = context->getProperty(MD5HashLocation, flow_file)) {
    if (auto md5_hash = flow_file->getAttribute(*md5_hash_location)) {
      callback.setHashValue(*md5_hash_location);
    }
  }

  if (auto content_type = getContentType(*context, *flow_file))
    callback.setContentType(*content_type);

  if (auto predefined_acl = context->getProperty<PredefinedAcl>(ObjectACL))
    callback.setPredefinedAcl(*predefined_acl);
  callback.setIfGenerationMatch(context->getProperty<bool>(OverwriteObject));

  callback.setEncryptionKey(encryption_key_);

  session->read(flow_file, &callback);
  auto& result = callback.getResult();
  if (!result.ok()) {
    flow_file->setAttribute(GCS_ERROR_REASON, result.status().error_info().reason());
    flow_file->setAttribute(GCS_ERROR_DOMAIN, result.status().error_info().domain());
    logger_->log_error("Failed to upload to Google Cloud Storage %s %s", result.status().message(), result.status().error_info().reason());
    session->transfer(flow_file, Failure);
  } else {
    setAttributesFromObjectMetadata(*flow_file, *result);
    session->transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(PutGcsObject, "Puts flow files to a Google Cloud Storage Bucket.");
}  // namespace org::apache::nifi::minifi::extensions::gcp
