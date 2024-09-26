/**
 * @file AWSCredentialsProvider.h
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

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aws/core/auth/AWSCredentials.h"
#include "utils/AWSInitializer.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {

class AWSCredentialsProvider {
 public:
  explicit AWSCredentialsProvider(
    bool use_default_credentials = false,
    const std::string &access_key = "",
    const std::string &secret_key = "",
    const std::string &credentials_file = "");
  void setUseDefaultCredentials(bool use_default_credentials);
  void setAccessKey(const std::string &access_key);
  void setSecretKey(const std::string &secret_key);
  void setCredentialsFile(const std::string &credentials_file);
  [[nodiscard]] bool getUseDefaultCredentials() const;
  [[nodiscard]] std::optional<Aws::Auth::AWSCredentials> getAWSCredentials();

 private:
  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  bool use_default_credentials_;
  std::string access_key_;
  std::string secret_key_;
  std::string credentials_file_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AWSCredentialsProvider>::getLogger()};
};

}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
