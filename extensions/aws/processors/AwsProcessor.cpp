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

#include "AwsProcessor.h"

#include <memory>
#include <string>
#include <utility>

#include "controllerservices/AWSCredentialsService.h"
#include "S3Wrapper.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/properties/Properties.h"
#include "range/v3/algorithm/contains.hpp"
#include "utils/HTTPUtils.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

std::optional<Aws::Auth::AWSCredentials> AwsProcessor::getAWSCredentialsFromControllerService(core::ProcessContext& context) const {
  if (auto service = minifi::utils::parseOptionalControllerService<controllers::AWSCredentialsService>(context, AWSCredentialsProviderService, getUUID())) {
    return service->getAWSCredentials();
  }
  logger_->log_error("AWS credentials service could not be found");
  return std::nullopt;
}

std::optional<Aws::Auth::AWSCredentials> AwsProcessor::getAWSCredentials(
    core::ProcessContext& context,
    const core::FlowFile* const flow_file) {
  auto service_cred = getAWSCredentialsFromControllerService(context);
  if (service_cred) {
    logger_->log_info("AWS Credentials successfully set from controller service");
    return service_cred;
  }

  aws::AWSCredentialsProvider aws_credentials_provider;
  if (const auto access_key = context.getProperty(AccessKey.name, flow_file)) {
    aws_credentials_provider.setAccessKey(*access_key);
  }
  if (const auto secret_key = context.getProperty(SecretKey.name, flow_file)) {
    aws_credentials_provider.setSecretKey(*secret_key);
  }
  if (const auto credentials_file = context.getProperty(CredentialsFile.name, flow_file)) {
    aws_credentials_provider.setCredentialsFile(*credentials_file);
  }
  if (const auto use_credentials = context.getProperty(UseDefaultCredentials.name, flow_file) | minifi::utils::andThen(parsing::parseBool)) {
    aws_credentials_provider.setUseDefaultCredentials(*use_credentials);
  }

  return aws_credentials_provider.getAWSCredentials();
}

minifi::controllers::ProxyConfiguration AwsProcessor::getProxy(core::ProcessContext& context, const core::FlowFile* const flow_file) {
  minifi::controllers::ProxyConfiguration proxy;

  auto proxy_controller_service = minifi::utils::parseOptionalControllerService<minifi::controllers::ProxyConfigurationServiceInterface>(context, ProxyConfigurationService, getUUID());
  if (proxy_controller_service) {
    proxy.proxy_type = proxy_controller_service->getProxyType();
    proxy.proxy_host = proxy_controller_service->getHost();
    auto port_opt = proxy_controller_service->getPort();
    proxy.proxy_port = port_opt.value_or(0);
    auto username_opt = proxy_controller_service->getUsername();
    proxy.proxy_user = username_opt.value_or("");
    auto password_opt = proxy_controller_service->getPassword();
    proxy.proxy_password = password_opt.value_or("");
  } else {
    proxy.proxy_type = minifi::utils::parseOptionalEnumProperty<minifi::controllers::ProxyType>(context, ProxyType).value_or(minifi::controllers::ProxyType::HTTP);
    proxy.proxy_host = minifi::utils::parseOptionalProperty(context, ProxyHost, flow_file).value_or("");
    proxy.proxy_port = gsl::narrow<uint32_t>(minifi::utils::parseOptionalU64Property(context, ProxyPort, flow_file).value_or(0));
    proxy.proxy_user = minifi::utils::parseOptionalProperty(context, ProxyUsername, flow_file).value_or("");
    proxy.proxy_password = minifi::utils::parseOptionalProperty(context, ProxyPassword, flow_file).value_or("");
  }

  if (!proxy.proxy_host.empty()) {
    logger_->log_info("Proxy for AwsProcessor was set.");
  }
  return proxy;
}

void AwsProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  client_config_ = Aws::Client::ClientConfiguration();

  client_config_->region = context.getProperty(Region) | minifi::utils::orThrow("Region property missing or invalid");
  logger_->log_debug("AwsProcessor: Region [{}]", client_config_->region);

  if (auto communications_timeout = minifi::utils::parseOptionalDurationProperty(context, CommunicationsTimeout)) {
    logger_->log_debug("AwsProcessor: Communications Timeout {}", *communications_timeout);
    client_config_->connectTimeoutMs = gsl::narrow<long>(communications_timeout->count());  // NOLINT(runtime/int,google-runtime-int)
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Communications Timeout missing or invalid");
  }

  static const auto default_ca_file = minifi::utils::getDefaultCAFile();
  if (default_ca_file) {
    client_config_->caFile = *default_ca_file;
  }

  // throw here if the credentials provider service is set to an invalid value
  std::ignore = minifi::utils::parseOptionalControllerService<controllers::AWSCredentialsService>(context, AWSCredentialsProviderService, getUUID());
}

std::optional<CommonProperties> AwsProcessor::getCommonELSupportedProperties(
    core::ProcessContext& context,
    const core::FlowFile* const flow_file) {
  CommonProperties properties;

  auto credentials = getAWSCredentials(context, flow_file);
  if (!credentials) {
    logger_->log_error("AWS Credentials have not been set!");
    return std::nullopt;
  }
  properties.credentials = credentials.value();
  properties.proxy = getProxy(context, flow_file);

  const auto endpoint_override_url = context.getProperty(EndpointOverrideURL, flow_file);
  if (endpoint_override_url) {
    properties.endpoint_override_url = *endpoint_override_url;
    logger_->log_debug("AwsProcessor: Endpoint Override URL [{}]", properties.endpoint_override_url);
  }

  return properties;
}

}  // namespace org::apache::nifi::minifi::aws::processors
