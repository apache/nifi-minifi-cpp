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
#include "http/HTTPClient.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "utils/ArrayUtils.h"
#include "utils/ResourceQueue.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::splunk {

class PutSplunkHTTP final : public SplunkHECProcessor {
 public:
  explicit PutSplunkHTTP(std::string_view name, const utils::Identifier& uuid = {})
      : SplunkHECProcessor(name, uuid) {
    logger_ = core::logging::LoggerFactory<PutSplunkHTTP>::getLogger(uuid_);
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

  EXTENSIONAPI static constexpr auto Source = core::PropertyDefinitionBuilder<>::createProperty("Source")
      .withDescription("Basic field describing the source of the event. If unspecified, the event will use the default defined in splunk.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SourceType = core::PropertyDefinitionBuilder<>::createProperty("Source Type")
      .withDescription("Basic field describing the source type of the event. If unspecified, the event will use the default defined in splunk.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Host = core::PropertyDefinitionBuilder<>::createProperty("Host")
      .withDescription("Basic field describing the host of the event. If unspecified, the event will use the default defined in splunk.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Index = core::PropertyDefinitionBuilder<>::createProperty("Index")
      .withDescription("Identifies the index where to send the event. If unspecified, the event will use the default defined in splunk.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ContentType = core::PropertyDefinitionBuilder<>::createProperty("Content Type")
      .withDescription("The media type of the event sent to Splunk. If not set, \"mime.type\" flow file attribute will be used. "
          "In case of neither of them is specified, this information will not be sent to the server.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(SplunkHECProcessor::Properties, std::to_array<core::PropertyReference>({
      Source,
      SourceType,
      Host,
      Index,
      ContentType
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles that are sent successfully to the destination are sent to this relationship."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles that failed to be sent to the destination are sent to this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 private:
  std::string getEndpoint(http::HTTPClient& client);

  std::shared_ptr<minifi::controllers::SSLContextServiceInterface> ssl_context_service_;
  std::optional<std::string> source_type_;
  std::optional<std::string> source_;
  std::optional<std::string> host_;
  std::optional<std::string> index_;
  std::shared_ptr<utils::ResourceQueue<http::HTTPClient>> client_queue_;
};

}  // namespace org::apache::nifi::minifi::extensions::splunk
