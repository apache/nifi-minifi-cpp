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
  logger_->log_debug("AWS credentials service could not be found");
  return std::nullopt;
}

std::optional<Aws::Auth::AWSCredentials> AwsProcessor::getAWSCredentials(core::ProcessContext& context) {
  auto service_cred = getAWSCredentialsFromControllerService(context);
  if (service_cred) {
    logger_->log_info("AWS Credentials successfully set from controller service");
    return service_cred;
  }

  aws::AWSCredentialsProvider aws_credentials_provider;
  if (const auto access_key = context.getProperty(AccessKey.name)) {
    aws_credentials_provider.setAccessKey(*access_key);
  }
  if (const auto secret_key = context.getProperty(SecretKey.name)) {
    aws_credentials_provider.setSecretKey(*secret_key);
  }
  if (const auto credentials_file = context.getProperty(CredentialsFile.name)) {
    aws_credentials_provider.setCredentialsFile(*credentials_file);
  }
  if (const auto use_credentials = context.getProperty(UseDefaultCredentials.name) | minifi::utils::andThen(parsing::parseBool)) {
    aws_credentials_provider.setUseDefaultCredentials(*use_credentials);
  }

  return aws_credentials_provider.getAWSCredentials();
}

minifi::controllers::ProxyConfiguration AwsProcessor::getProxy(core::ProcessContext& context) {
  minifi::controllers::ProxyConfiguration proxy;

  auto proxy_controller_service = minifi::utils::parseOptionalControllerService<minifi::controllers::ProxyConfigurationServiceInterface>(context, ProxyConfigurationService, getUUID());
  if (proxy_controller_service) {
    proxy.proxy_type = proxy_controller_service->getProxyType();
    proxy.proxy_host = proxy_controller_service->getHost();
    proxy.proxy_port = proxy_controller_service->getPort();
    proxy.proxy_credentials = proxy_controller_service->getProxyCredentials();
  } else {
    proxy.proxy_host = minifi::utils::parseOptionalProperty(context, ProxyHost).value_or("");
    proxy.proxy_type = proxy.proxy_host.empty() ? minifi::controllers::ProxyType::DIRECT : minifi::controllers::ProxyType::HTTP;
    proxy.proxy_port = gsl::narrow<uint32_t>(minifi::utils::parseOptionalU64Property(context, ProxyPort).value_or(0));
    auto proxy_user = minifi::utils::parseOptionalProperty(context, ProxyUsername).value_or("");
    if (!proxy_user.empty()) {
      proxy.proxy_credentials = minifi::controllers::BasicAuthCredentials{.username = proxy_user, .password = minifi::utils::parseOptionalProperty(context, ProxyPassword).value_or("")};
    }
  }

  if (!proxy.proxy_host.empty()) {
    logger_->log_info("Proxy for AwsProcessor was set.");
  }
  return proxy;
}

void AwsProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  client_config_ = Aws::Client::ClientConfiguration();

  client_config_.region = context.getProperty(Region) | minifi::utils::orThrow("Region property missing or invalid");
  logger_->log_debug("AwsProcessor: Region [{}]", client_config_.region);

  if (auto communications_timeout = minifi::utils::parseOptionalDurationProperty(context, CommunicationsTimeout)) {
    logger_->log_debug("AwsProcessor: Communications Timeout {}", *communications_timeout);
    client_config_.connectTimeoutMs = gsl::narrow<long>(communications_timeout->count());  // NOLINT(runtime/int,google-runtime-int)
    client_config_.requestTimeoutMs = gsl::narrow<long>(communications_timeout->count());  // NOLINT(runtime/int,google-runtime-int)
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Communications Timeout missing or invalid");
  }

  static const auto default_ca_file = minifi::utils::getDefaultCAFile();
  if (default_ca_file) {
    client_config_.caFile = *default_ca_file;
  }

  // throw here if the credentials provider service is set to an invalid value
  std::ignore = minifi::utils::parseOptionalControllerService<controllers::AWSCredentialsService>(context, AWSCredentialsProviderService, getUUID());

  auto credentials = getAWSCredentials(context);
  if (!credentials) {
    logger_->log_error("AWS Credentials have not been set!");
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "AWS Credentials have not been set!");
  }
  credentials_ = credentials.value();

  auto proxy = getProxy(context);
  if (proxy.proxy_type != minifi::controllers::ProxyType::DIRECT) {
    if (minifi::utils::string::startsWith(proxy.proxy_host, "https://")) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "HTTPS proxy is not supported");
    }
    client_config_.proxyScheme = Aws::Http::Scheme::HTTP;
    client_config_.proxyHost = proxy.proxy_host;
    client_config_.proxyPort = proxy.proxy_port;
    if (proxy.proxy_credentials) {
      client_config_.proxyUserName = proxy.proxy_credentials->username;
      client_config_.proxyPassword = proxy.proxy_credentials->password;
    }
  }

  const auto endpoint_override_url = context.getProperty(EndpointOverrideURL);
  if (endpoint_override_url) {
    client_config_.endpointOverride = *endpoint_override_url;
    logger_->log_debug("AwsProcessor: Endpoint Override URL [{}]", *endpoint_override_url);
  }
}

}  // namespace org::apache::nifi::minifi::aws::processors
