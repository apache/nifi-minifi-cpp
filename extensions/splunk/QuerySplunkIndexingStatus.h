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
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "utils/ArrayUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "http/HTTPClient.h"
#include "rapidjson/stringbuffer.h"

namespace org::apache::nifi::minifi::extensions::splunk {

class QuerySplunkIndexingStatus final : public SplunkHECProcessor {
 public:
  using SplunkHECProcessor::SplunkHECProcessor;
  QuerySplunkIndexingStatus(const QuerySplunkIndexingStatus&) = delete;
  QuerySplunkIndexingStatus(QuerySplunkIndexingStatus&&) = delete;
  QuerySplunkIndexingStatus& operator=(const QuerySplunkIndexingStatus&) = delete;
  QuerySplunkIndexingStatus& operator=(QuerySplunkIndexingStatus&&) = delete;
  ~QuerySplunkIndexingStatus() override = default;

  EXTENSIONAPI static constexpr const char* Description =
      "Queries the Splunk server in order to acquire the status of indexing acknowledgement.\n"
      "\n"
      "This processor is responsible for polling Splunk server and determine if a Splunk event is acknowledged at the time of\n"
      "execution. For more details about the HEC Index Acknowledgement please see\n"
      "https://docs.splunk.com/Documentation/Splunk/LATEST/Data/AboutHECIDXAck.\n"
      "\n"
      "In order to work properly, the incoming flow files need to have the attributes \"splunk.acknowledgement.id\" and\n"
      "\"splunk.responded.at\" filled properly. The flow file attribute \"splunk.acknowledgement.id\" should contain the \"ackId\"\n"
      "which can be extracted from the response to the original Splunk put call. The flow file attribute \"splunk.responded.at\"\n"
      "should contain the timestamp describing when the put call was answered by Splunk.\n"
      "These required attributes are set by PutSplunkHTTP processor.\n"
      "\n"
      "Undetermined cases are normal in healthy environment as it is possible that minifi asks for indexing status before Splunk\n"
      "finishes and acknowledges it. These cases are safe to retry, and it is suggested to loop \"undetermined\" relationship\n"
      "back to the processor for later try. Flow files transferred into the \"Undetermined\" relationship are penalized.\n"
      "\n"
      "Please keep Splunk channel limitations in mind: there are multiple configuration parameters in Splunk which might have direct\n"
      "effect on the performance and behaviour of the QuerySplunkIndexingStatus processor. For example \"max_number_of_acked_requests_pending_query\"\n"
      "and \"max_number_of_acked_requests_pending_query_per_ack_channel\" might limit the amount of ackIDs Splunk stores.\n"
      "\n"
      "Also, it is suggested to execute the query in batches. The \"Maximum Query Size\" property might be used for fine tune\n"
      "the maximum number of events the processor will query about in one API request. This serves as an upper limit for the\n"
      "batch but the processor might execute the query with smaller number of undetermined events.\n";

  EXTENSIONAPI static constexpr auto MaximumWaitingTime = core::PropertyDefinitionBuilder<>::createProperty("Maximum Waiting Time")
      .withDescription("The maximum time the processor tries to acquire acknowledgement confirmation for an index, from the point of registration. "
          "After the given amount of time, the processor considers the index as not acknowledged and transfers the FlowFile to the \"unacknowledged\" relationship.")
      .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
      .withDefaultValue("1 hour")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxQuerySize = core::PropertyDefinitionBuilder<>::createProperty("Maximum Query Size")
      .withDescription("The maximum number of acknowledgement identifiers the outgoing query contains in one batch. "
          "It is recommended not to set it too low in order to reduce network communication.")
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("1000")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(SplunkHECProcessor::Properties, std::to_array<core::PropertyReference>({
      MaximumWaitingTime,
      MaxQuerySize
  }));


  EXTENSIONAPI static constexpr auto Acknowledged = core::RelationshipDefinition{"acknowledged",
    "A FlowFile is transferred to this relationship when the acknowledgement was successful."};
  EXTENSIONAPI static constexpr auto Unacknowledged = core::RelationshipDefinition{"unacknowledged",
    "A FlowFile is transferred to this relationship when the acknowledgement was not successful. "
    "This can happen when the acknowledgement did not happened within the time period set for Maximum Waiting Time. "
    "FlowFiles with acknowledgement id unknown for the Splunk server will be transferred to this relationship after the Maximum Waiting Time is reached."};
  EXTENSIONAPI static constexpr auto Undetermined = core::RelationshipDefinition{"undetermined",
    "A FlowFile is transferred to this relationship when the acknowledgement state is not determined. "
    "FlowFiles transferred to this relationship might be penalized. "
    "This happens when Splunk returns with HTTP 200 but with false response for the acknowledgement id in the flow file attribute."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
    "A FlowFile is transferred to this relationship when the acknowledgement was not successful due to errors during the communication, "
    "or if the flowfile was missing the acknowledgement id"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{
      Acknowledged,
      Unacknowledged,
      Undetermined,
      Failure
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  uint64_t batch_size_ = 1000;
  std::chrono::milliseconds max_age_ = std::chrono::hours(1);
  http::HTTPClient client_;
};

}  // namespace org::apache::nifi::minifi::extensions::splunk
