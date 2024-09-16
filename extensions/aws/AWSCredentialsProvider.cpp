/**
 * @file AWSCredentialsProvider.cpp
 * AWSCredentialsProvider class implementation
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

#include "AWSCredentialsProvider.h"

#include "aws/core/auth/AWSCredentialsProviderChain.h"
#include "properties/Properties.h"

namespace org::apache::nifi::minifi::aws {

AWSCredentialsProvider::AWSCredentialsProvider(
    bool use_default_credentials,
    std::string access_key,
    std::string secret_key,
    std::string credentials_file)
  : use_default_credentials_(use_default_credentials)
  , access_key_(std::move(access_key))
  , secret_key_(std::move(secret_key))
  , credentials_file_(std::move(credentials_file)) {
}

void AWSCredentialsProvider::setUseDefaultCredentials(bool use_default_credentials) {
  use_default_credentials_ = use_default_credentials;
}

bool AWSCredentialsProvider::getUseDefaultCredentials() const {
  return use_default_credentials_;
}

void AWSCredentialsProvider::setAccessKey(const std::string &access_key) {
  access_key_ = access_key;
}

void AWSCredentialsProvider::setSecretKey(const std::string &secret_key) {
  secret_key_ = secret_key;
}

void AWSCredentialsProvider::setCredentialsFile(const std::string &credentials_file) {
  credentials_file_ = credentials_file;
}

std::optional<Aws::Auth::AWSCredentials> AWSCredentialsProvider::getAWSCredentials() {
  if (use_default_credentials_) {
    logger_->log_debug("Trying to use default AWS credentials provider chain.");
    auto creds = Aws::Auth::DefaultAWSCredentialsProviderChain().GetAWSCredentials();
    if (!creds.GetAWSAccessKeyId().empty() || !creds.GetAWSSecretKey().empty() || !creds.GetSessionToken().empty()) {
      logger_->log_debug("AWS credentials found on the default AWS credentials provider chain.");
      return creds;
    }
    logger_->log_debug("No credentials were found through the default AWS credentials provider chain.");
  }

  if (!access_key_.empty() && !secret_key_.empty()) {
    logger_->log_debug("Using access key and secret key as AWS credentials.");
    return Aws::Auth::AWSCredentials(access_key_, secret_key_);
  }

  if (!credentials_file_.empty()) {
    auto properties = minifi::Properties::create();
    properties->loadConfigureFile(credentials_file_.c_str());
    std::string access_key;
    std::string secret_key;
    if (properties->getString("accessKey", access_key) && !access_key.empty() && properties->getString("secretKey", secret_key) && !secret_key.empty()) {
      logger_->log_debug("Using AWS credentials from credentials file.");
      return Aws::Auth::AWSCredentials(access_key, secret_key);
    }
  }

  logger_->log_debug("No AWS credentials were set.");
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::aws
