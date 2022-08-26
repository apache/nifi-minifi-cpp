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
#include <vector>

#include "controllers/SSLContextService.h"
#include "ElasticsearchCredentialsControllerService.h"
#include "core/Processor.h"
#include "utils/Enum.h"
#include "client/HTTPClient.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

class PostElasticsearch : public core::Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "An Elasticsearch/Opensearch post processor that uses the Elasticsearch/Opensearch _bulk REST API.";

  explicit PostElasticsearch(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
  }
  ~PostElasticsearch() override = default;

  EXTENSIONAPI static const core::Property Action;
  EXTENSIONAPI static const core::Property MaxBatchSize;
  EXTENSIONAPI static const core::Property ElasticCredentials;
  EXTENSIONAPI static const core::Property SSLContext;
  EXTENSIONAPI static const core::Property Hosts;
  EXTENSIONAPI static const core::Property Index;
  EXTENSIONAPI static const core::Property Identifier;

  static auto properties() {
    return std::array{
      Action,
      MaxBatchSize,
      ElasticCredentials,
      SSLContext,
      Hosts,
      Index,
      Identifier
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  EXTENSIONAPI static const core::Relationship Error;

  static auto relationships() {
    return std::array{Success, Failure, Error};
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

 private:
  std::string collectPayload(core::ProcessContext&, core::ProcessSession&, std::vector<std::shared_ptr<core::FlowFile>>&) const;

  uint64_t max_batch_size_ = 100;
  std::string host_url_;
  std::shared_ptr<ElasticsearchCredentialsControllerService> credentials_service_;
  curl::HTTPClient client_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PostElasticsearch>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
