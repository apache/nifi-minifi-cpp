/**
 * @file PutS3Object.cpp
 * PutS3Object class implementation
 *
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
#include "PutS3Object.h"
#include "AWSCredentialsService.h"
#include "properties/Properties.h"

#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const core::Property PutS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->withDefaultValue<std::string>("${filename}")
    ->build());
const core::Property PutS3Object::Bucket(
  core::PropertyBuilder::createProperty("Bucket")
    ->withDescription("The S3 bucket")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ContentType(
  core::PropertyBuilder::createProperty("Content Type")
    ->withDescription("Sets the Content-Type HTTP header indicating the type of content stored in "
                      "the associated object. The value of this header is a standard MIME type."
                      " AWS S3 client will attempt to determine the correct content type if "
                      "one hasn't been set yet. Users are responsible for ensuring a suitable "
                      "content type is set when uploading streams. If no content type is provided"
                      " and cannot be determined by the filename, the default content type "
                      "\"application/octet-stream\" will be used.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::AccessKey(
  core::PropertyBuilder::createProperty("Access Key")
    ->withDescription("AWS credential access key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::SecretKey(
  core::PropertyBuilder::createProperty("Secret Key")
    ->withDescription("AWS credential secret key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::CredentialsFile(
  core::PropertyBuilder::createProperty("Credentials File")
    ->withDescription("Path to a file containing AWS access key and secret key in properties file format.")
    ->build());
const core::Property PutS3Object::AWSCredentialsProviderService(
  core::PropertyBuilder::createProperty("AWS Credentials Provider service")
    ->withDescription("The Controller Service that is used to obtain aws credentials provider")
    ->build());
const core::Property PutS3Object::StorageClass(
  core::PropertyBuilder::createProperty("Storage Class")
    ->isRequired(true)
    ->withDefaultValue<std::string>(storage_class::STANDARD)
    ->withAllowableValues<std::string>({storage_class::STANDARD, storage_class::REDUCED_REDUNDANCY})
    ->withDescription("")
    ->build());
const core::Property PutS3Object::Region(
  core::PropertyBuilder::createProperty("Region")
    ->isRequired(true)
    ->withDefaultValue<std::string>(region::US_WEST_2)
    ->withAllowableValues<std::string>({region::US_GOV_WEST_1, region::US_EAST_1, region::US_EAST_2, region::US_WEST_1,
      region::US_WEST_2, region::EU_WEST_1, region::EU_WEST_2, region::EU_CENTRAL_1, region::AP_SOUTH_1,
      region::AP_SOUTHEAST_1, region::AP_SOUTHEAST_2, region::AP_NORTHEAST_1, region::AP_NORTHEAST_2,
      region::SA_EAST_1, region::CN_NORTH_1, region::CA_CENTRAL_1})
    ->withDescription("")
    ->build());
const core::Property PutS3Object::CommunicationsTimeout(
  core::PropertyBuilder::createProperty("Communications Timeout")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("30 sec")
    ->withDescription("")
    ->build());
const core::Property PutS3Object::EndpointOverrideURL(
  core::PropertyBuilder::createProperty("Endpoint Override URL")
    ->withDescription("Endpoint URL to use instead of the AWS default including scheme, host, "
                      "port, and path. The AWS libraries select an endpoint URL based on the AWS "
                      "region, but this property overrides the selected endpoint URL, allowing use "
                      "with other S3-compatible endpoints.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ServerSideEncryption(
  core::PropertyBuilder::createProperty("Server Side Encryption")
    ->isRequired(true)
    ->withDefaultValue<std::string>(server_side_encryption::NONE)
    ->withAllowableValues<std::string>({server_side_encryption::NONE, server_side_encryption::AES256, server_side_encryption::AWS_KMS})
    ->withDescription("Specifies the algorithm used for server side encryption.")
    ->build());

const core::Relationship PutS3Object::Success("success", "FlowFiles are routed to success relationship");
const core::Relationship PutS3Object::Failure("failure", "FlowFiles are routed to failure relationship");

void PutS3Object::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(ObjectKey);
  properties.insert(Bucket);
  properties.insert(ContentType);
  properties.insert(AccessKey);
  properties.insert(SecretKey);
  properties.insert(CredentialsFile);
  properties.insert(AWSCredentialsProviderService);
  properties.insert(StorageClass);
  properties.insert(Region);
  properties.insert(CommunicationsTimeout);
  properties.insert(EndpointOverrideURL);
  properties.insert(ServerSideEncryption);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

minifi::utils::optional<Aws::Auth::AWSCredentials> PutS3Object::getAWSCredentialsFromControllerService(const std::shared_ptr<core::ProcessContext> &context) {
  std::string service_name;
  if (context->getProperty(AWSCredentialsProviderService.getName(), service_name) && !IsNullOrEmpty(service_name)) {
    std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(service_name);
    if (nullptr != service) {
      auto aws_credentials_service = std::static_pointer_cast<minifi::aws::controllers::AWSCredentialsService>(service);
      return minifi::utils::make_optional<Aws::Auth::AWSCredentials>(aws_credentials_service->getAWSCredentials());
    }
  }
  return minifi::utils::nullopt;
}

minifi::utils::optional<Aws::Auth::AWSCredentials> PutS3Object::getAWSCredentialsFromProperties(const std::shared_ptr<core::ProcessContext> &context) {
  std::string access_key;
  context->getProperty(AccessKey.getName(), access_key);
  std::string secret_key;
  context->getProperty(SecretKey.getName(), secret_key);
  if (!IsNullOrEmpty(access_key) && !IsNullOrEmpty(secret_key)) {
    Aws::Auth::AWSCredentials creds(access_key, secret_key);
    return minifi::utils::make_optional<Aws::Auth::AWSCredentials>(creds);
  }
  return minifi::utils::nullopt;
}

minifi::utils::optional<Aws::Auth::AWSCredentials> PutS3Object::getAWSCredentialsFromFile(const std::shared_ptr<core::ProcessContext> &context) {
  std::string credential_file;
  if (context->getProperty(CredentialsFile.getName(), credential_file) && !IsNullOrEmpty(credential_file)) {
    auto properties = std::make_shared<minifi::Properties>();
    properties->loadConfigureFile(credential_file.c_str());
    std::string access_key;
    std::string secret_key;
    if (properties->get("accessKey", access_key) && !IsNullOrEmpty(access_key) && properties->get("secretKey", secret_key) && !IsNullOrEmpty(secret_key)) {
      Aws::Auth::AWSCredentials creds(access_key, secret_key);
      return minifi::utils::make_optional<Aws::Auth::AWSCredentials>(creds);
    }
  }
  return minifi::utils::nullopt;
}

Aws::Auth::AWSCredentials PutS3Object::getAWSCredentials(const std::shared_ptr<core::ProcessContext> &context) {
  auto service_cred = getAWSCredentialsFromControllerService(context);
  if (service_cred) {
    logger_->log_info("AWS Credentials successfully set from controller service");
    return service_cred.value();
  }

  auto prop_cred = getAWSCredentialsFromProperties(context);
  if (prop_cred) {
    logger_->log_info("AWS Credentials successfully set from properties");
    return prop_cred.value();
  }

  auto file_cred = getAWSCredentialsFromFile(context);
  if (file_cred) {
    logger_->log_info("AWS Credentials successfully set from file");
    return file_cred.value();
  }

  throw Exception(PROCESS_SCHEDULE_EXCEPTION, "No AWS credentials are set");
}

void PutS3Object::fillUserMetadata(const std::shared_ptr<core::ProcessContext> &context) {
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  bool first_property = true;
  for (const auto &prop_key : dynamic_prop_keys) {
    std::string prop_value = "";
    if (context->getDynamicProperty(prop_key, prop_value) && !prop_value.empty()) {
      logger_->log_debug("PutS3Object: DynamicProperty: [%s] -> [%s]", prop_key, prop_value);
      if (first_property) {
        user_metadata_ = prop_key + "=" + prop_value;
        first_property = false;
      } else {
        user_metadata_ += "," + prop_key + "=" + prop_value;
      }
    }
  }
}

void PutS3Object::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  context->getProperty(ObjectKey.getName(), object_key_);
  if (!context->getProperty(Bucket.getName(), bucket_) || bucket_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bucket property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Bucket [%s]", bucket_);

  context->getProperty(ContentType.getName(), content_type_);
  logger_->log_debug("PutS3Object: Content Type [%s]", content_type_);

  s3_wrapper_->setCredentials(getAWSCredentials(context));
  if (!context->getProperty(StorageClass.getName(), storage_class_) || storage_class_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Storage Class property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Storage Class [%s]", storage_class_);

  std::string value;
  if (!context->getProperty(Region.getName(), value) || value.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Region property missing or invalid");
  }
  s3_wrapper_->setRegion(value);
  logger_->log_debug("PutS3Object: Region [%s]", value);

  uint64_t timeout_val;
  context->getProperty(CommunicationsTimeout.getName(), value);
  if (core::Property::getTimeMSFromString(value, timeout_val)) {
    s3_wrapper_->setTimeout(timeout_val);
    logger_->log_debug("PutS3Object: Communications Timeout [%d]", timeout_val);
  }

  if (context->getProperty(EndpointOverrideURL.getName(), value) && !value.empty()) {
    s3_wrapper_->setEndpointOverrideUrl(value);
    logger_->log_debug("PutS3Object: Endpoint Override URL [%d]", value);
  }

  if (!context->getProperty(ServerSideEncryption.getName(), server_side_encryption_) || server_side_encryption_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Storage Class property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Server Side Encryption [%s]", server_side_encryption_);

  fillUserMetadata(context);
}

void PutS3Object::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_debug("PutS3Object onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }

  std::string filename;
  flow_file->getAttribute("filename", filename);
  object_key_ = minifi::utils::StringUtils::replaceOne(object_key_, "${filename}", filename);
  session->putAttribute(flow_file, "s3.bucket", bucket_);
  session->putAttribute(flow_file, "s3.key", object_key_);
  session->putAttribute(flow_file, "s3.contenttype", content_type_);
  session->putAttribute(flow_file, "s3.usermetadata", user_metadata_);

  minifi::aws::s3::PutS3ObjectOptions options{bucket_, object_key_, storage_class_, server_side_encryption_};
  PutS3Object::ReadCallback callback(flow_file->getSize(), std::move(options), s3_wrapper_.get());
  session->read(flow_file, &callback);
  if (callback.result_ == minifi::utils::nullopt) {
    logger_->log_error("Failed to send flow to S3 bucket %s", bucket_);
    session->transfer(flow_file, Failure);
  } else {
    session->putAttribute(flow_file, "s3.version", callback.result_.value().version);
    session->putAttribute(flow_file, "s3.etag", callback.result_.value().etag);
    session->putAttribute(flow_file, "s3.expiration", callback.result_.value().expiration);
    session->putAttribute(flow_file, "s3.sseAlgorithm", callback.result_.value().ssealgorithm);

    logger_->log_debug("Sent S3 object %s to bucket %s", object_key_, bucket_);
    session->transfer(flow_file, Success);
  }
}

void PutS3Object::notifyStop() {

}

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
