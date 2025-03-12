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

#include <memory>
#include <string>
#include <utility>

#include "AWSCredentialsService.h"
#include "S3Wrapper.h"
#include "core/ProcessContext.h"
#include "properties/Properties.h"
#include "range/v3/algorithm/contains.hpp"
#include "utils/HTTPUtils.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

S3Processor::S3Processor(std::string_view name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
  : core::ProcessorImpl(name, uuid),
    logger_(std::move(logger)) {
}

S3Processor::S3Processor(std::string_view name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
  : core::ProcessorImpl(name, uuid),
    logger_(std::move(logger)),
    s3_wrapper_(std::move(s3_request_sender)) {
}

std::optional<Aws::Auth::AWSCredentials> S3Processor::getAWSCredentialsFromControllerService(core::ProcessContext& context) const {
  if (const auto aws_credentials_service = minifi::utils::parseOptionalControllerService<controllers::AWSCredentialsService>(context, AWSCredentialsProviderService, getUUID())) {
    return (*aws_credentials_service)->getAWSCredentials();
  }
  logger_->log_error("AWS credentials service could not be found");

  return std::nullopt;
}

std::optional<Aws::Auth::AWSCredentials> S3Processor::getAWSCredentials(
    core::ProcessContext& context,
    const core::FlowFile* const flow_file) {
  auto service_cred = getAWSCredentialsFromControllerService(context);
  if (service_cred) {
    logger_->log_info("AWS Credentials successfully set from controller service");
    return service_cred.value();
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

std::optional<aws::s3::ProxyOptions> S3Processor::getProxy(core::ProcessContext& context, const core::FlowFile* const flow_file) {
  aws::s3::ProxyOptions proxy;

  proxy.host = minifi::utils::parseOptionalProperty(context, ProxyHost, flow_file).value_or("");
  proxy.port = gsl::narrow<uint32_t>(minifi::utils::parseOptionalU64Property(context, ProxyPort, flow_file).value_or(0));
  proxy.username = minifi::utils::parseOptionalProperty(context, ProxyUsername, flow_file).value_or("");
  proxy.password = minifi::utils::parseOptionalProperty(context, ProxyPassword, flow_file).value_or("");

  if (!proxy.host.empty()) {
    logger_->log_info("Proxy for S3Processor was set.");
  }
  return proxy;
}

void S3Processor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  client_config_ = Aws::Client::ClientConfiguration();
  if (!getProperty(Bucket.name)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bucket property missing or invalid");
  }

  client_config_->region = context.getProperty(Region) | minifi::utils::expect("Region property missing or invalid");
  logger_->log_debug("S3Processor: Region [{}]", client_config_->region);

  if (auto communications_timeout = minifi::utils::parseOptionalDurationProperty(context, CommunicationsTimeout)) {
    logger_->log_debug("S3Processor: Communications Timeout {}", *communications_timeout);
    client_config_->connectTimeoutMs = gsl::narrow<long>(communications_timeout->count());  // NOLINT(runtime/int,google-runtime-int)
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Communications Timeout missing or invalid");
  }

  static const auto default_ca_file = minifi::utils::getDefaultCAFile();
  if (default_ca_file) {
    client_config_->caFile = *default_ca_file;
  }
}

std::optional<CommonProperties> S3Processor::getCommonELSupportedProperties(
    core::ProcessContext& context,
    const core::FlowFile* const flow_file) {
  CommonProperties properties;
  if (auto bucket = context.getProperty(Bucket, flow_file); !bucket || bucket->empty()) {
    logger_->log_error("Bucket '{}' is invalid or empty!", properties.bucket);
    return std::nullopt;
  } else {
    properties.bucket = *bucket;
  }
  logger_->log_debug("S3Processor: Bucket [{}]", properties.bucket);

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

  const auto endpoint_override_url = context.getProperty(EndpointOverrideURL, flow_file);
  if (endpoint_override_url) {
    properties.endpoint_override_url = *endpoint_override_url;
    logger_->log_debug("S3Processor: Endpoint Override URL [{}]", properties.endpoint_override_url);
  }

  return properties;
}

}  // namespace org::apache::nifi::minifi::aws::processors
