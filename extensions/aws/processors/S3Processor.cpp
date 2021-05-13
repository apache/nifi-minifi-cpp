/**
 * @file S3Processor.cpp
 * Base S3 processor class implementation
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

#include "S3Processor.h"

#include <string>
#include <set>
#include <memory>

#include "S3Wrapper.h"
#include "AWSCredentialsService.h"
#include "properties/Properties.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const std::set<std::string> S3Processor::REGIONS({region::AF_SOUTH_1, region::AP_EAST_1, region::AP_NORTHEAST_1,
  region::AP_NORTHEAST_2, region::AP_NORTHEAST_3, region::AP_SOUTH_1, region::AP_SOUTHEAST_1, region::AP_SOUTHEAST_2,
  region::CA_CENTRAL_1, region::CN_NORTH_1, region::CN_NORTHWEST_1, region::EU_CENTRAL_1, region::EU_NORTH_1,
  region::EU_SOUTH_1, region::EU_WEST_1, region::EU_WEST_2, region::EU_WEST_3, region::ME_SOUTH_1, region::SA_EAST_1,
  region::US_EAST_1, region::US_EAST_2, region::US_GOV_EAST_1, region::US_GOV_WEST_1, region::US_WEST_1, region::US_WEST_2});

const core::Property S3Processor::Bucket(
  core::PropertyBuilder::createProperty("Bucket")
    ->withDescription("The S3 bucket")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::AccessKey(
  core::PropertyBuilder::createProperty("Access Key")
    ->withDescription("AWS account access key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::SecretKey(
  core::PropertyBuilder::createProperty("Secret Key")
    ->withDescription("AWS account secret key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::CredentialsFile(
  core::PropertyBuilder::createProperty("Credentials File")
    ->withDescription("Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey")
    ->build());
const core::Property S3Processor::AWSCredentialsProviderService(
  core::PropertyBuilder::createProperty("AWS Credentials Provider service")
    ->withDescription("The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.")
    ->build());
const core::Property S3Processor::Region(
  core::PropertyBuilder::createProperty("Region")
    ->isRequired(true)
    ->withDefaultValue<std::string>(region::US_WEST_2)
    ->withAllowableValues<std::string>(S3Processor::REGIONS)
    ->withDescription("AWS Region")
    ->build());
const core::Property S3Processor::CommunicationsTimeout(
  core::PropertyBuilder::createProperty("Communications Timeout")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("30 sec")
    ->withDescription("Sets the timeout of the communication between the AWS server and the client")
    ->build());
const core::Property S3Processor::EndpointOverrideURL(
  core::PropertyBuilder::createProperty("Endpoint Override URL")
    ->withDescription("Endpoint URL to use instead of the AWS default including scheme, host, "
                      "port, and path. The AWS libraries select an endpoint URL based on the AWS "
                      "region, but this property overrides the selected endpoint URL, allowing use "
                      "with other S3-compatible endpoints.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyHost(
  core::PropertyBuilder::createProperty("Proxy Host")
    ->withDescription("Proxy host name or IP")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyPort(
  core::PropertyBuilder::createProperty("Proxy Port")
    ->withDescription("The port number of the proxy host")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyUsername(
    core::PropertyBuilder::createProperty("Proxy Username")
    ->withDescription("Username to set when authenticating against proxy")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyPassword(
  core::PropertyBuilder::createProperty("Proxy Password")
    ->withDescription("Password to set when authenticating against proxy")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::UseDefaultCredentials(
    core::PropertyBuilder::createProperty("Use Default Credentials")
    ->withDescription("If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.")
    ->withDefaultValue<bool>(false)
    ->isRequired(true)
    ->build());

S3Processor::S3Processor(const std::string& name, const minifi::utils::Identifier& uuid, const std::shared_ptr<logging::Logger> &logger)
  : core::Processor(name, uuid)
  , logger_(logger) {
  setSupportedProperties({Bucket, AccessKey, SecretKey, CredentialsFile, CredentialsFile, AWSCredentialsProviderService, Region, CommunicationsTimeout,
                          EndpointOverrideURL, ProxyHost, ProxyPort, ProxyUsername, ProxyPassword, UseDefaultCredentials});
}

S3Processor::S3Processor(const std::string& name, const minifi::utils::Identifier& uuid, const std::shared_ptr<logging::Logger> &logger, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
  : core::Processor(name, uuid)
  , logger_(logger)
  , s3_wrapper_(std::move(s3_request_sender)) {
  setSupportedProperties({Bucket, AccessKey, SecretKey, CredentialsFile, CredentialsFile, AWSCredentialsProviderService, Region, CommunicationsTimeout,
                          EndpointOverrideURL, ProxyHost, ProxyPort, ProxyUsername, ProxyPassword, UseDefaultCredentials});
}

minifi::utils::optional<Aws::Auth::AWSCredentials> S3Processor::getAWSCredentialsFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const {
  std::string service_name;
  if (!context->getProperty(AWSCredentialsProviderService.getName(), service_name) || service_name.empty()) {
    return minifi::utils::nullopt;
  }

  std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(service_name);
  if (!service) {
    logger_->log_error("AWS credentials service with name: '%s' could not be found", service_name);
    return minifi::utils::nullopt;
  }

  auto aws_credentials_service = std::dynamic_pointer_cast<minifi::aws::controllers::AWSCredentialsService>(service);
  if (!aws_credentials_service) {
    logger_->log_error("Controller service with name: '%s' is not an AWS credentials service", service_name);
    return minifi::utils::nullopt;
  }

  return aws_credentials_service->getAWSCredentials();
}

minifi::utils::optional<Aws::Auth::AWSCredentials> S3Processor::getAWSCredentials(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  auto service_cred = getAWSCredentialsFromControllerService(context);
  if (service_cred) {
    logger_->log_info("AWS Credentials successfully set from controller service");
    return service_cred.value();
  }

  aws::AWSCredentialsProvider aws_credentials_provider;
  std::string value;
  if (context->getProperty(AccessKey, value, flow_file)) {
    aws_credentials_provider.setAccessKey(value);
  }
  if (context->getProperty(SecretKey, value, flow_file)) {
    aws_credentials_provider.setSecretKey(value);
  }
  if (context->getProperty(CredentialsFile.getName(), value)) {
    aws_credentials_provider.setCredentialsFile(value);
  }
  bool use_default_credentials = false;
  if (context->getProperty(UseDefaultCredentials.getName(), use_default_credentials)) {
    aws_credentials_provider.setUseDefaultCredentials(use_default_credentials);
  }

  return aws_credentials_provider.getAWSCredentials();
}

minifi::utils::optional<aws::s3::ProxyOptions> S3Processor::getProxy(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  aws::s3::ProxyOptions proxy;
  context->getProperty(ProxyHost, proxy.host, flow_file);
  std::string port_str;
  if (context->getProperty(ProxyPort, port_str, flow_file) && !port_str.empty() && !core::Property::StringToInt(port_str, proxy.port)) {
    logger_->log_error("Proxy port invalid");
    return minifi::utils::nullopt;
  }
  context->getProperty(ProxyUsername, proxy.username, flow_file);
  context->getProperty(ProxyPassword, proxy.password, flow_file);
  if (!proxy.host.empty()) {
    logger_->log_info("Proxy for S3Processor was set.");
  }
  return proxy;
}

void S3Processor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  std::string value;
  if (!context->getProperty(Bucket.getName(), value) || value.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bucket property missing or invalid");
  }

  if (!context->getProperty(Region.getName(), client_config_.region) || client_config_.region.empty() || REGIONS.count(client_config_.region) == 0) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Region property missing or invalid");
  }
  logger_->log_debug("S3Processor: Region [%s]", client_config_.region);

  uint64_t timeout_val;
  if (context->getProperty(CommunicationsTimeout.getName(), value) && !value.empty() && core::Property::getTimeMSFromString(value, timeout_val)) {
    logger_->log_debug("S3Processor: Communications Timeout [%llu]", timeout_val);
    client_config_.connectTimeoutMs = gsl::narrow<int64_t>(timeout_val);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Communications Timeout missing or invalid");
  }
}

minifi::utils::optional<CommonProperties> S3Processor::getCommonELSupportedProperties(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  CommonProperties properties;
  if (!context->getProperty(Bucket, properties.bucket, flow_file) || properties.bucket.empty()) {
    logger_->log_error("Bucket '%s' is invalid or empty!", properties.bucket);
    return minifi::utils::nullopt;
  }
  logger_->log_debug("S3Processor: Bucket [%s]", properties.bucket);

  auto credentials = getAWSCredentials(context, flow_file);
  if (!credentials) {
    logger_->log_error("AWS Credentials have not been set!");
    return minifi::utils::nullopt;
  }
  properties.credentials = credentials.value();

  auto proxy = getProxy(context, flow_file);
  if (!proxy) {
    return minifi::utils::nullopt;
  }
  properties.proxy = proxy.value();

  context->getProperty(EndpointOverrideURL, properties.endpoint_override_url, flow_file);
  if (!properties.endpoint_override_url.empty()) {
    logger_->log_debug("S3Processor: Endpoint Override URL [%s]", properties.endpoint_override_url);
  }

  return properties;
}

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
