// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aws/core/auth/AWSCredentials.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::aws {

class AWSCredentialsProvider {
 public:
  explicit AWSCredentialsProvider(
    bool use_default_credentials = false,
    std::string access_key = "",
    std::string secret_key = "",
    std::string credentials_file = "");
  void setUseDefaultCredentials(bool use_default_credentials);
  void setAccessKey(const std::string &access_key);
  void setSecretKey(const std::string &secret_key);
  void setCredentialsFile(const std::string &credentials_file);
  [[nodiscard]] bool getUseDefaultCredentials() const;
  [[nodiscard]] std::optional<Aws::Auth::AWSCredentials> getAWSCredentials();

 private:
  bool use_default_credentials_;
  std::string access_key_;
  std::string secret_key_;
  std::string credentials_file_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AWSCredentialsProvider>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::aws
