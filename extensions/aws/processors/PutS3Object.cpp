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

#include <string>
#include <set>
#include <memory>
#include <map>

#include "AWSCredentialsService.h"
#include "properties/Properties.h"
#include "utils/StringUtils.h"
#include "utils/MapUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const uint64_t PutS3Object::ReadCallback::MAX_SIZE = 5UL * 1024UL * 1024UL * 1024UL;  // 5GB limit on AWS
const uint64_t PutS3Object::ReadCallback::BUFFER_SIZE = 4096;

const std::set<std::string> PutS3Object::CANNED_ACLS(minifi::utils::MapUtils::getKeys(minifi::aws::s3::CANNED_ACL_MAP));
const std::set<std::string> PutS3Object::REGIONS({region::AF_SOUTH_1, region::AP_EAST_1, region::AP_NORTHEAST_1,
  region::AP_NORTHEAST_2, region::AP_NORTHEAST_3, region::AP_SOUTH_1, region::AP_SOUTHEAST_1, region::AP_SOUTHEAST_2,
  region::CA_CENTRAL_1, region::CN_NORTH_1, region::CN_NORTHWEST_1, region::EU_CENTRAL_1, region::EU_NORTH_1,
  region::EU_SOUTH_1, region::EU_WEST_1, region::EU_WEST_2, region::EU_WEST_3, region::ME_SOUTH_1, region::SA_EAST_1,
  region::US_EAST_1, region::US_EAST_2, region::US_GOV_EAST_1, region::US_GOV_WEST_1, region::US_WEST_1, region::US_WEST_2});
const std::set<std::string> PutS3Object::STORAGE_CLASSES(minifi::utils::MapUtils::getKeys(minifi::aws::s3::STORAGE_CLASS_MAP));
const std::set<std::string> PutS3Object::SERVER_SIDE_ENCRYPTIONS(minifi::utils::MapUtils::getKeys(minifi::aws::s3::SERVER_SIDE_ENCRYPTION_MAP));

const core::Property PutS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
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
                      "the associated object. The value of this header is a standard MIME type. "
                      "If no content type is provided the default content type "
                      "\"application/octet-stream\" will be used.")
    ->supportsExpressionLanguage(true)
    ->withDefaultValue<std::string>("application/octet-stream")
    ->build());
const core::Property PutS3Object::AccessKey(
  core::PropertyBuilder::createProperty("Access Key")
    ->withDescription("AWS account access key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::SecretKey(
  core::PropertyBuilder::createProperty("Secret Key")
    ->withDescription("AWS account secret key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::CredentialsFile(
  core::PropertyBuilder::createProperty("Credentials File")
    ->withDescription("Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey")
    ->build());
const core::Property PutS3Object::AWSCredentialsProviderService(
  core::PropertyBuilder::createProperty("AWS Credentials Provider service")
    ->withDescription("The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.")
    ->build());
const core::Property PutS3Object::StorageClass(
  core::PropertyBuilder::createProperty("Storage Class")
    ->isRequired(true)
    ->withDefaultValue<std::string>("Standard")
    ->withAllowableValues<std::string>(PutS3Object::STORAGE_CLASSES)
    ->withDescription("AWS S3 Storage Class")
    ->build());
const core::Property PutS3Object::Region(
  core::PropertyBuilder::createProperty("Region")
    ->isRequired(true)
    ->withDefaultValue<std::string>(region::US_WEST_2)
    ->withAllowableValues<std::string>(PutS3Object::REGIONS)
    ->withDescription("AWS Region")
    ->build());
const core::Property PutS3Object::CommunicationsTimeout(
  core::PropertyBuilder::createProperty("Communications Timeout")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("30 sec")
    ->withDescription("")
    ->build());
const core::Property PutS3Object::FullControlUserList(
  core::PropertyBuilder::createProperty("FullControl User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Full Control for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ReadPermissionUserList(
  core::PropertyBuilder::createProperty("Read Permission User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Read Access for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ReadACLUserList(
  core::PropertyBuilder::createProperty("Read ACL User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to read "
                      "the Access Control List for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::WriteACLUserList(
  core::PropertyBuilder::createProperty("Write ACL User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to change "
                      "the Access Control List for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::CannedACL(
  core::PropertyBuilder::createProperty("Canned ACL")
    ->withDescription("Amazon Canned ACL for an object. Allowed values: BucketOwnerFullControl, BucketOwnerRead, AuthenticatedRead, "
                      "PublicReadWrite, PublicRead, Private, AwsExecRead; will be ignored if any other ACL/permission property is specified.")
    ->supportsExpressionLanguage(true)
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
    ->withDefaultValue<std::string>("None")
    ->withAllowableValues<std::string>(PutS3Object::SERVER_SIDE_ENCRYPTIONS)
    ->withDescription("Specifies the algorithm used for server side encryption.")
    ->build());
const core::Property PutS3Object::ProxyHost(
  core::PropertyBuilder::createProperty("Proxy Host")
    ->withDescription("Proxy host name or IP")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ProxyPort(
  core::PropertyBuilder::createProperty("Proxy Port")
    ->withDescription("The port number of the proxy host")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ProxyUsername(
    core::PropertyBuilder::createProperty("Proxy Username")
    ->withDescription("Username to set when authenticating against proxy")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ProxyPassword(
  core::PropertyBuilder::createProperty("Proxy Password")
    ->withDescription("Password to set when authenticating against proxy")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::UseDefaultCredentials(
    core::PropertyBuilder::createProperty("Use Default Credentials")
    ->withDescription("If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.")
    ->withDefaultValue<bool>(false)
    ->isRequired(true)
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
  properties.insert(FullControlUserList);
  properties.insert(ReadPermissionUserList);
  properties.insert(ReadACLUserList);
  properties.insert(WriteACLUserList);
  properties.insert(CannedACL);
  properties.insert(EndpointOverrideURL);
  properties.insert(ServerSideEncryption);
  properties.insert(ProxyHost);
  properties.insert(ProxyPort);
  properties.insert(ProxyUsername);
  properties.insert(ProxyPassword);
  properties.insert(UseDefaultCredentials);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

minifi::utils::optional<Aws::Auth::AWSCredentials> PutS3Object::getAWSCredentialsFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const {
  std::string service_name;
  if (context->getProperty(AWSCredentialsProviderService.getName(), service_name) && !service_name.empty()) {
    std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(service_name);
    if (nullptr != service) {
      auto aws_credentials_service = std::dynamic_pointer_cast<minifi::aws::controllers::AWSCredentialsService>(service);
      if (aws_credentials_service) {
        return minifi::utils::make_optional<Aws::Auth::AWSCredentials>(aws_credentials_service->getAWSCredentials());
      }
    }
  }
  return minifi::utils::nullopt;
}

minifi::utils::optional<Aws::Auth::AWSCredentials> PutS3Object::getAWSCredentials(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  auto service_cred = getAWSCredentialsFromControllerService(context);
  if (service_cred) {
    logger_->log_info("AWS Credentials successfully set from controller service");
    return service_cred.value();
  }

  std::string access_key;
  context->getProperty(AccessKey, access_key, flow_file);
  aws_credentials_provider_.setAccessKey(access_key);
  std::string secret_key;
  context->getProperty(SecretKey, secret_key, flow_file);
  aws_credentials_provider_.setSecretKey(secret_key);
  std::string credential_file;
  context->getProperty(CredentialsFile.getName(), credential_file);
  aws_credentials_provider_.setCredentialsFile(credential_file);
  bool use_default_credentials = false;
  context->getProperty(UseDefaultCredentials.getName(), use_default_credentials);
  aws_credentials_provider_.setUseDefaultCredentials(use_default_credentials);

  return aws_credentials_provider_.getAWSCredentials();
}

void PutS3Object::fillUserMetadata(const std::shared_ptr<core::ProcessContext> &context) {
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  bool first_property = true;
  for (const auto &prop_key : dynamic_prop_keys) {
    std::string prop_value = "";
    if (context->getDynamicProperty(prop_key, prop_value) && !prop_value.empty()) {
      logger_->log_debug("PutS3Object: DynamicProperty: [%s] -> [%s]", prop_key, prop_value);
      put_s3_request_params_.user_metadata_map.emplace(prop_key, prop_value);
      if (first_property) {
        user_metadata_ = prop_key + "=" + prop_value;
        first_property = false;
      } else {
        user_metadata_ += "," + prop_key + "=" + prop_value;
      }
    }
  }
  logger_->log_debug("PutS3Object: User metadata [%s]", user_metadata_);
}

bool PutS3Object::setProxy(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  aws::s3::ProxyOptions proxy;
  context->getProperty(ProxyHost, proxy.host, flow_file);
  std::string port_str;
  if (context->getProperty(ProxyPort, port_str, flow_file) && !port_str.empty() && !core::Property::StringToInt(port_str, proxy.port)) {
    logger_->log_error("Proxy port invalid");
    return false;
  }
  context->getProperty(ProxyUsername, proxy.username, flow_file);
  context->getProperty(ProxyPassword, proxy.password, flow_file);
  if (!proxy.host.empty()) {
    s3_wrapper_->setProxy(proxy);
    logger_->log_info("Proxy for PutS3Object was set.");
  }
  return true;
}

void PutS3Object::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (!context->getProperty(Bucket.getName(), put_s3_request_params_.bucket) || put_s3_request_params_.bucket.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bucket property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Bucket [%s]", put_s3_request_params_.bucket);

  if (!context->getProperty(StorageClass.getName(), put_s3_request_params_.storage_class)
      || put_s3_request_params_.storage_class.empty()
      || STORAGE_CLASSES.find(put_s3_request_params_.storage_class) == STORAGE_CLASSES.end()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Storage Class property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Storage Class [%s]", put_s3_request_params_.storage_class);

  std::string value;
  if (!context->getProperty(Region.getName(), value) || value.empty() || REGIONS.count(value) == 0) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Region property missing or invalid");
  }
  s3_wrapper_->setRegion(value);
  logger_->log_debug("PutS3Object: Region [%s]", value);

  uint64_t timeout_val;
  if (context->getProperty(CommunicationsTimeout.getName(), value) && !value.empty() && core::Property::getTimeMSFromString(value, timeout_val)) {
    s3_wrapper_->setTimeout(timeout_val);
    logger_->log_debug("PutS3Object: Communications Timeout [%d]", timeout_val);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Communications Timeout missing or invalid");
  }

  if (!context->getProperty(ServerSideEncryption.getName(), put_s3_request_params_.server_side_encryption)
      || put_s3_request_params_.server_side_encryption.empty()
      || SERVER_SIDE_ENCRYPTIONS.find(put_s3_request_params_.server_side_encryption) == SERVER_SIDE_ENCRYPTIONS.end()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Server Side Encryption property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Server Side Encryption [%s]", put_s3_request_params_.server_side_encryption);

  fillUserMetadata(context);
}

std::string PutS3Object::parseAccessControlList(const std::string &comma_separated_list) const {
  auto users = minifi::utils::StringUtils::split(comma_separated_list, ",");
  for (auto& user : users) {
    auto trimmed_user = minifi::utils::StringUtils::trim(user);
    if (trimmed_user.find('@') != std::string::npos) {
      user = "emailAddress=\"" + trimmed_user + "\"";
    } else {
      user = "id=" + trimmed_user;
    }
  }
  return minifi::utils::StringUtils::join(", ", users);
}

bool PutS3Object::setCannedAcl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  context->getProperty(CannedACL, put_s3_request_params_.canned_acl, flow_file);
  if (!put_s3_request_params_.canned_acl.empty() && CANNED_ACLS.find(put_s3_request_params_.canned_acl) == CANNED_ACLS.end()) {
    logger_->log_error("Canned ACL is invalid!");
    return false;
  }
  logger_->log_debug("PutS3Object: Canned ACL [%s]", put_s3_request_params_.canned_acl);
  return true;
}

bool PutS3Object::setAccessControl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  std::string value;
  if (context->getProperty(FullControlUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params_.fullcontrol_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Full Control User List [%s]", value);
  }
  if (context->getProperty(ReadPermissionUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params_.read_permission_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Read Permission User List [%s]", value);
  }
  if (context->getProperty(ReadACLUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params_.read_acl_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Read ACL User List [%s]", value);
  }
  if (context->getProperty(WriteACLUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params_.write_acl_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Write ACL User List [%s]", value);
  }

  return setCannedAcl(context, flow_file);
}

bool PutS3Object::getExpressionLanguageSupportedProperties(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  context->getProperty(ObjectKey, put_s3_request_params_.object_key, flow_file);
  if (put_s3_request_params_.object_key.empty()) {
    if (!flow_file->getAttribute("filename", put_s3_request_params_.object_key) || put_s3_request_params_.object_key.empty()) {
      logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
      return false;
    }
  }
  logger_->log_debug("PutS3Object: Object Key [%s]", put_s3_request_params_.object_key);
  if (!context->getProperty(Bucket, put_s3_request_params_.bucket, flow_file) || put_s3_request_params_.bucket.empty()) {
    logger_->log_error("Bucket is invalid or empty!", put_s3_request_params_.bucket);
    return false;
  }
  logger_->log_debug("PutS3Object: Bucket [%s]", put_s3_request_params_.bucket);

  context->getProperty(ContentType, put_s3_request_params_.content_type, flow_file);
  logger_->log_debug("PutS3Object: Content Type [%s]", put_s3_request_params_.content_type);

  auto credentials = getAWSCredentials(context, flow_file);
  if (!credentials) {
    logger_->log_error("AWS Credentials have not been set!");
    return false;
  }
  s3_wrapper_->setCredentials(credentials.value());

  if (!setProxy(context, flow_file)) {
    context->yield();
    return false;
  }

  std::string value;
  if (context->getProperty(EndpointOverrideURL, value, flow_file) && !value.empty()) {
    s3_wrapper_->setEndpointOverrideUrl(value);
    logger_->log_debug("PutS3Object: Endpoint Override URL [%d]", value);
  }

  return setAccessControl(context, flow_file);
}

void PutS3Object::setAttributes(
    const std::shared_ptr<core::ProcessSession> &session,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const minifi::aws::s3::PutObjectResult &put_object_result) {
  session->putAttribute(flow_file, "s3.bucket", put_s3_request_params_.bucket);
  session->putAttribute(flow_file, "s3.key", put_s3_request_params_.object_key);
  session->putAttribute(flow_file, "s3.contenttype", put_s3_request_params_.content_type);

  if (!user_metadata_.empty()) {
    session->putAttribute(flow_file, "s3.usermetadata", user_metadata_);
  }
  if (!put_object_result.version.empty()) {
    session->putAttribute(flow_file, "s3.version", put_object_result.version);
  }
  if (!put_object_result.etag.empty()) {
    session->putAttribute(flow_file, "s3.etag", put_object_result.etag);
  }
  if (!put_object_result.expiration.empty()) {
    session->putAttribute(flow_file, "s3.expiration", put_object_result.expiration);
  }
  if (!put_object_result.ssealgorithm.empty()) {
    session->putAttribute(flow_file, "s3.sseAlgorithm", put_object_result.ssealgorithm);
  }
}

void PutS3Object::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_debug("PutS3Object onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }

  if (!getExpressionLanguageSupportedProperties(context, flow_file)) {
    context->yield();
    return;
  }

  PutS3Object::ReadCallback callback(flow_file->getSize(), put_s3_request_params_, s3_wrapper_.get());
  session->read(flow_file, &callback);
  if (callback.result_ == minifi::utils::nullopt) {
    logger_->log_error("Failed to upload S3 object to bucket %s", put_s3_request_params_.bucket);
    session->transfer(flow_file, Failure);
  } else {
    setAttributes(session, flow_file, callback.result_.value());
    logger_->log_debug("Successfully uploaded S3 object %s to bucket %s", put_s3_request_params_.object_key, put_s3_request_params_.bucket);
    session->transfer(flow_file, Success);
  }
}

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
