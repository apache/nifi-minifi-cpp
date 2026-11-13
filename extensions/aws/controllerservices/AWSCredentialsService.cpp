// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "AWSCredentialsService.h"

#include "core/Resource.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::aws::controllers {

void AWSCredentialsService::initialize() {
  setSupportedProperties(Properties);
}

void AWSCredentialsService::onEnable() {
  if (const auto access_key = getProperty(AccessKey.name)) {
    aws_credentials_provider_.setAccessKey(*access_key);
  }
  if (const auto secret_key = getProperty(SecretKey.name)) {
    aws_credentials_provider_.setSecretKey(*secret_key);
  }
  if (const auto credentials_file = getProperty(CredentialsFile.name)) {
    aws_credentials_provider_.setCredentialsFile(*credentials_file);
  }
  if (const auto use_credentials = getProperty(UseDefaultCredentials.name) | minifi::utils::andThen(parsing::parseBool)) {
    aws_credentials_provider_.setUseDefaultCredentials(*use_credentials);
  }
}

std::optional<Aws::Auth::AWSCredentials> AWSCredentialsService::getAWSCredentials() {
  if (aws_credentials_provider_.getUseDefaultCredentials() || !aws_credentials_ || aws_credentials_->IsExpiredOrEmpty()) {
    aws_credentials_ = aws_credentials_provider_.getAWSCredentials();
  }
  return aws_credentials_;
}

REGISTER_RESOURCE(AWSCredentialsService, ControllerService);

}  // namespace org::apache::nifi::minifi::aws::controllers
