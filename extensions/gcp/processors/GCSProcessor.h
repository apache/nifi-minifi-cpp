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


#pragma once

#include <string>
#include <memory>
#include <utility>
#include <optional>

#include "core/logging/Logger.h"
#include "core/Processor.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/retry_policy.h"

namespace org::apache::nifi::minifi::extensions::gcp {
class GCSProcessor : public core::Processor {
 public:
  GCSProcessor(std::string name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
      : core::Processor(std::move(name), uuid),
        logger_(std::move(logger)) {
  }

  EXTENSIONAPI static const core::Property GCPCredentials;
  EXTENSIONAPI static const core::Property NumberOfRetries;
  EXTENSIONAPI static const core::Property EndpointOverrideURL;
  static auto properties() {
    return std::array{
      GCPCredentials,
      NumberOfRetries,
      EndpointOverrideURL
    };
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 protected:
  virtual google::cloud::storage::Client getClient() const;

  std::optional<std::string> endpoint_url_;
  std::shared_ptr<google::cloud::storage::oauth2::Credentials> gcp_credentials_;
  google::cloud::storage::RetryPolicyOption::Type retry_policy_ = std::make_shared<google::cloud::storage::LimitedErrorCountRetryPolicy>(6);
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
