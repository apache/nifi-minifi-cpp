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
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"
#include "client/HTTPClient.h"
#include "rapidjson/stringbuffer.h"

namespace org::apache::nifi::minifi::extensions::splunk {

class QuerySplunkIndexingStatus final : public SplunkHECProcessor {
 public:
  explicit QuerySplunkIndexingStatus(std::string name, const utils::Identifier& uuid = {})
      : SplunkHECProcessor(std::move(name), uuid) {
  }
  QuerySplunkIndexingStatus(const QuerySplunkIndexingStatus&) = delete;
  QuerySplunkIndexingStatus(QuerySplunkIndexingStatus&&) = delete;
  QuerySplunkIndexingStatus& operator=(const QuerySplunkIndexingStatus&) = delete;
  QuerySplunkIndexingStatus& operator=(QuerySplunkIndexingStatus&&) = delete;
  ~QuerySplunkIndexingStatus() override = default;

  EXTENSIONAPI static constexpr const char* Description =
      "Queries the Splunk server in order to acquire the status of indexing acknowledgement."
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

  EXTENSIONAPI static const core::Property MaximumWaitingTime;
  EXTENSIONAPI static const core::Property MaxQuerySize;
  static auto properties() {
    return utils::array_cat(SplunkHECProcessor::properties(), std::array{
      MaximumWaitingTime,
      MaxQuerySize
    });
  }

  EXTENSIONAPI static const core::Relationship Acknowledged;
  EXTENSIONAPI static const core::Relationship Unacknowledged;
  EXTENSIONAPI static const core::Relationship Undetermined;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() {
    return std::array{
      Acknowledged,
      Unacknowledged,
      Undetermined,
      Failure
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 protected:
  uint32_t batch_size_ = 1000;
  std::chrono::milliseconds max_age_ = std::chrono::hours(1);
  curl::HTTPClient client_;
};

}  // namespace org::apache::nifi::minifi::extensions::splunk
