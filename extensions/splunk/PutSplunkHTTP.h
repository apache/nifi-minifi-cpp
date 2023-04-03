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

  EXTENSIONAPI static constexpr const char* Description =
      "Sends the flow file contents to the specified Splunk HTTP Event Collector (see https://docs.splunk.com/Documentation/SplunkCloud/latest/Data/UsetheHTTPEventCollector) over HTTP or HTTPS.\n"
      "\n"
      "The \"Source\", \"Source Type\", \"Host\" and \"Index\" properties are optional and will be set by Splunk if unspecified. If set,\n"
      "the default values will be overwritten with the user specified ones. For more details about the Splunk API, please visit\n"
      "[this documentation](https://docs.splunk.com/Documentation/Splunk/LATEST/RESTREF/RESTinput#services.2Fcollector.2Fraw)\n"
      "\n"
      "HTTP Event Collector (HEC) in Splunk provides the possibility of index acknowledgement, which can be used to monitor\n"
      "the indexing status of the individual events. PutSplunkHTTP supports this feature by enriching the outgoing flow file\n"
      "with the necessary information, making it possible for a later processor to poll the status based on. The necessary\n"
      "information for this is stored within flow file attributes \"splunk.acknowledgement.id\" and \"splunk.responded.at\".\n"
      "\n"
      "For more refined processing, flow files are enriched with additional information if possible. The information is stored\n"
      "in the flow file attribute \"splunk.status.code\" or \"splunk.response.code\", depending on the success of the processing.\n"
      "The attribute \"splunk.status.code\" is always filled when the Splunk API call is executed and contains the HTTP status code\n"
      "of the response. In case the flow file transferred into \"failure\" relationship, the \"splunk.response.code\" might be\n"
      "also filled, based on the Splunk response code.";

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
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<PutSplunkHTTP>::getLogger(uuid_)};
  std::shared_ptr<utils::ResourceQueue<extensions::curl::HTTPClient>> client_queue_;
};

}  // namespace org::apache::nifi::minifi::extensions::splunk

