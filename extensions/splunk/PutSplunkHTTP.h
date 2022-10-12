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

#include <memory>
#include <string>
#include <utility>

#include "SplunkHECProcessor.h"
#include "client/HTTPClient.h"
#include "utils/ArrayUtils.h"
#include "utils/ResourceQueue.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::splunk {

class PutSplunkHTTP final : public SplunkHECProcessor {
 public:
  explicit PutSplunkHTTP(std::string name, const utils::Identifier& uuid = {})
      : SplunkHECProcessor(std::move(name), uuid) {
  }
  PutSplunkHTTP(const PutSplunkHTTP&) = delete;
  PutSplunkHTTP(PutSplunkHTTP&&) = delete;
  PutSplunkHTTP& operator=(const PutSplunkHTTP&) = delete;
  PutSplunkHTTP& operator=(PutSplunkHTTP&&) = delete;
  ~PutSplunkHTTP() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Sends the flow file contents to the specified Splunk HTTP Event Collector over HTTP or HTTPS. Supports HEC Index Acknowledgement.";

  EXTENSIONAPI static const core::Property Source;
  EXTENSIONAPI static const core::Property SourceType;
  EXTENSIONAPI static const core::Property Host;
  EXTENSIONAPI static const core::Property Index;
  EXTENSIONAPI static const core::Property ContentType;
  static auto properties() {
    return utils::array_cat(SplunkHECProcessor::properties(), std::array{
      Source,
      SourceType,
      Host,
      Index,
      ContentType
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<PutSplunkHTTP>::getLogger()};
  std::shared_ptr<utils::ResourceQueue<extensions::curl::HTTPClient>> client_queue_;
};

}  // namespace org::apache::nifi::minifi::extensions::splunk

