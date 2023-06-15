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

#include "S3Processor.h"

#include <string>
#include <memory>
#include <utility>

#include "core/ProcessContext.h"
#include "S3Wrapper.h"
#include "AWSCredentialsService.h"
#include "properties/Properties.h"
#include "range/v3/algorithm/contains.hpp"
#include "utils/StringUtils.h"
#include "utils/HTTPUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

S3Processor::S3Processor(std::string name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
  : core::Processor(std::move(name), uuid),
    logger_(std::move(logger)) {
}

S3Processor::S3Processor(std::string name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
  : core::Processor(std::move(name), uuid),
    logger_(std::move(logger)),
    s3_wrapper_(std::move(s3_request_sender)) {
}

std::optional<Aws::Auth::AWSCredentials> S3Processor::getAWSCredentialsFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const {
  std::string service_name;
  if (!context->getProperty(AWSCredentialsProviderService, service_name) || service_name.empty()) {
    return std::nullopt;
  }

  std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(service_name);
  if (!service) {
    logger_->log_error("AWS credentials service with name: '%s' could not be found", service_name);
    return std::nullopt;
  }

  auto aws_credentials_service = std::dynamic_pointer_cast<minifi::aws::controllers::AWSCredentialsService>(service);
  if (!aws_credentials_service) {
    logger_->log_error("Controller service with name: '%s' is not an AWS credentials service", service_name);
    return std::nullopt;
  }

  return aws_credentials_service->getAWSCredentials();
}

std::optional<Aws::Auth::AWSCredentials> S3Processor::getAWSCredentials(
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
  if (context->getProperty(CredentialsFile, value)) {
    aws_credentials_provider.setCredentialsFile(value);
  }
  bool use_default_credentials = false;
  if (context->getProperty(UseDefaultCredentials, use_default_credentials)) {
    aws_credentials_provider.setUseDefaultCredentials(use_default_credentials);
  }

  return aws_credentials_provider.getAWSCredentials();
}

std::optional<aws::s3::ProxyOptions> S3Processor::getProxy(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  aws::s3::ProxyOptions proxy;
  context->getProperty(ProxyHost, proxy.host, flow_file);
  std::string port_str;
  if (context->getProperty(ProxyPort, port_str, flow_file) && !port_str.empty() && !core::Property::StringToInt(port_str, proxy.port)) {
    logger_->log_error("Proxy port invalid");
    return std::nullopt;
  }
  context->getProperty(ProxyUsername, proxy.username, flow_file);
  context->getProperty(ProxyPassword, proxy.password, flow_file);
  if (!proxy.host.empty()) {
    logger_->log_info("Proxy for S3Processor was set.");
  }
  return proxy;
}

void S3Processor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  client_config_ = Aws::Client::ClientConfiguration();
  std::string value;
  if (!context->getProperty(Bucket, value) || value.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bucket property missing or invalid");
  }

  if (!context->getProperty(Region, client_config_->region) || client_config_->region.empty() || !ranges::contains(region::REGIONS, client_config_->region)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Region property missing or invalid");
  }
  logger_->log_debug("S3Processor: Region [%s]", client_config_->region);

  if (auto communications_timeout = context->getProperty<core::TimePeriodValue>(CommunicationsTimeout)) {
    logger_->log_debug("S3Processor: Communications Timeout %" PRId64 " ms", communications_timeout->getMilliseconds().count());
    client_config_->connectTimeoutMs = gsl::narrow<int64_t>(communications_timeout->getMilliseconds().count());
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Communications Timeout missing or invalid");
  }

  static const auto default_ca_path = minifi::utils::getDefaultCAPath();
  if (default_ca_path) {
    client_config_->caFile = default_ca_path->string();
  }
}

std::optional<CommonProperties> S3Processor::getCommonELSupportedProperties(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  CommonProperties properties;
  if (!context->getProperty(Bucket, properties.bucket, flow_file) || properties.bucket.empty()) {
    logger_->log_error("Bucket '%s' is invalid or empty!", properties.bucket);
    return std::nullopt;
  }
  logger_->log_debug("S3Processor: Bucket [%s]", properties.bucket);

  auto credentials = getAWSCredentials(context, flow_file);
  if (!credentials) {
    logger_->log_error("AWS Credentials have not been set!");
    return std::nullopt;
  }
  properties.credentials = credentials.value();

  auto proxy = getProxy(context, flow_file);
  if (!proxy) {
    return std::nullopt;
  }
  properties.proxy = proxy.value();

  context->getProperty(EndpointOverrideURL, properties.endpoint_override_url, flow_file);
  if (!properties.endpoint_override_url.empty()) {
    logger_->log_debug("S3Processor: Endpoint Override URL [%s]", properties.endpoint_override_url);
  }

  return properties;
}

}  // namespace org::apache::nifi::minifi::aws::processors
