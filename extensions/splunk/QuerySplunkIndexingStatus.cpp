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


#include "QuerySplunkIndexingStatus.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "SplunkAttributes.h"

#include "core/Resource.h"
#include "client/HTTPClient.h"
#include "utils/HTTPClient.h"

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace org::apache::nifi::minifi::extensions::splunk {

const core::Property QuerySplunkIndexingStatus::MaximumWaitingTime(core::PropertyBuilder::createProperty("Maximum Waiting Time")
    ->withDescription("The maximum time the processor tries to acquire acknowledgement confirmation for an index, from the point of registration. "
                      "After the given amount of time, the processor considers the index as not acknowledged and transfers the FlowFile to the \"unacknowledged\" relationship.")
    ->withDefaultValue<core::TimePeriodValue>("1 hour")->isRequired(true)->build());

const core::Property QuerySplunkIndexingStatus::MaxQuerySize(core::PropertyBuilder::createProperty("Maximum Query Size")
    ->withDescription("The maximum number of acknowledgement identifiers the outgoing query contains in one batch. "
                      "It is recommended not to set it too low in order to reduce network communication.")
    ->withDefaultValue<uint64_t>(1000)->isRequired(true)->build());

const core::Relationship QuerySplunkIndexingStatus::Acknowledged("acknowledged",
    "A FlowFile is transferred to this relationship when the acknowledgement was successful.");

const core::Relationship QuerySplunkIndexingStatus::Unacknowledged("unacknowledged",
    "A FlowFile is transferred to this relationship when the acknowledgement was not successful. "
    "This can happen when the acknowledgement did not happened within the time period set for Maximum Waiting Time. "
    "FlowFiles with acknowledgement id unknown for the Splunk server will be transferred to this relationship after the Maximum Waiting Time is reached.");

const core::Relationship QuerySplunkIndexingStatus::Undetermined("undetermined",
    "A FlowFile is transferred to this relationship when the acknowledgement state is not determined. "
    "FlowFiles transferred to this relationship might be penalized. "
    "This happens when Splunk returns with HTTP 200 but with false response for the acknowledgement id in the flow file attribute.");

const core::Relationship QuerySplunkIndexingStatus::Failure("failure",
    "A FlowFile is transferred to this relationship when the acknowledgement was not successful due to errors during the communication, "
    "or if the flowfile was missing the acknowledgement id");

void QuerySplunkIndexingStatus::initialize() {
  setSupportedRelationships({Acknowledged, Unacknowledged, Undetermined, Failure});
  setSupportedProperties({Hostname, Port, Token, SplunkRequestChannel, SSLContext, MaximumWaitingTime, MaxQuerySize});
}

void QuerySplunkIndexingStatus::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) {
  gsl_Expects(context && sessionFactory);
  SplunkHECProcessor::onSchedule(context, sessionFactory);
  std::string max_wait_time_str;
  if (context->getProperty(MaximumWaitingTime.getName(), max_wait_time_str)) {
    core::TimeUnit unit;
    uint64_t max_wait_time;
    if (core::Property::StringToTime(max_wait_time_str, max_wait_time, unit) && core::Property::ConvertTimeUnitToMS(max_wait_time, unit, max_wait_time)) {
      max_age_ = std::chrono::milliseconds(max_wait_time);
    }
  }

  context->getProperty(MaxQuerySize.getName(), batch_size_);
}

namespace {
constexpr std::string_view getEndpoint() {
  return "/services/collector/ack";
}

struct FlowFileWithIndexStatus {
  explicit FlowFileWithIndexStatus(gsl::not_null<std::shared_ptr<core::FlowFile>>&& flow_file) : flow_file_(std::move(flow_file)) {}

  gsl::not_null<std::shared_ptr<core::FlowFile>> flow_file_;
  std::optional<bool> indexing_status_ = std::nullopt;
};

std::unordered_map<uint64_t, FlowFileWithIndexStatus> getUndeterminedFlowFiles(core::ProcessSession& session, size_t batch_size) {
  std::unordered_map<uint64_t, FlowFileWithIndexStatus> undetermined_flow_files;
  std::unordered_set<uint64_t> duplicate_ack_ids;
  for (size_t i = 0; i < batch_size; ++i) {
    auto flow = session.get();
    if (flow == nullptr)
      break;
    std::optional<std::string> splunk_ack_id_str = flow->getAttribute(SPLUNK_ACK_ID);
    if (!splunk_ack_id_str.has_value()) {
      session.transfer(flow, QuerySplunkIndexingStatus::Failure);
      continue;
    }
    uint64_t splunk_ack_id = std::stoull(splunk_ack_id_str.value());
    if (undetermined_flow_files.contains(splunk_ack_id)) {
      duplicate_ack_ids.insert(splunk_ack_id);
      session.transfer(flow, QuerySplunkIndexingStatus::Failure);
      continue;
    }
    undetermined_flow_files.emplace(std::make_pair(splunk_ack_id, gsl::not_null(std::move(flow))));
  }
  for (auto duplicate_ack_id : duplicate_ack_ids) {
    session.transfer(undetermined_flow_files.at(duplicate_ack_id).flow_file_, QuerySplunkIndexingStatus::Failure);
    undetermined_flow_files.erase(duplicate_ack_id);
  }
  return undetermined_flow_files;
}

std::string getAckIdsAsPayload(const std::unordered_map<uint64_t, FlowFileWithIndexStatus>& undetermined_flow_files) {
  rapidjson::Document payload = rapidjson::Document(rapidjson::kObjectType);
  payload.AddMember("acks", rapidjson::kArrayType, payload.GetAllocator());
  for (const auto& [ack_id, ff_status] : undetermined_flow_files) {
    payload["acks"].PushBack(ack_id, payload.GetAllocator());
  }
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  payload.Accept(writer);
  return buffer.GetString();
}

void getIndexingStatusFromSplunk(utils::HTTPClient& client, std::unordered_map<uint64_t, FlowFileWithIndexStatus>& undetermined_flow_files) {
  rapidjson::Document response;
  if (!client.submit())
    return;
  if (client.getResponseCode() != 200)
    return;
  response = rapidjson::Document();
  rapidjson::ParseResult parse_result = response.Parse<rapidjson::kParseStopWhenDoneFlag>(client.getResponseBody().data());
  if (parse_result.IsError() || !response.HasMember("acks"))
    return;

  rapidjson::Value& acks = response["acks"];
  for (auto& [ack_id, ff_status]: undetermined_flow_files) {
    if (acks.HasMember(std::to_string(ack_id).c_str()) && acks[std::to_string(ack_id).c_str()].IsBool())
      ff_status.indexing_status_ = acks[std::to_string(ack_id).c_str()].GetBool();
  }
}

bool flowFileAcknowledgementTimedOut(const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file, std::chrono::milliseconds max_age) {
  using std::chrono::system_clock;
  using std::chrono::milliseconds;
  std::optional<std::string> splunk_response_time_str = flow_file->getAttribute(SPLUNK_RESPONSE_TIME);
  if (!splunk_response_time_str.has_value())
    return true;
  uint64_t splunk_response_time = std::stoull(splunk_response_time_str.value());
  if (system_clock::now() > std::chrono::system_clock::time_point() + std::chrono::milliseconds(splunk_response_time) + max_age)
    return true;
  return false;
}

void routeFlowFilesBasedOnIndexingStatus(core::ProcessSession& session,
                                         const std::unordered_map<uint64_t, FlowFileWithIndexStatus>& flow_files_with_index_statuses,
                                         std::chrono::milliseconds max_age) {
  for (const auto& [ack_id, ff_status] : flow_files_with_index_statuses) {
    if (!ff_status.indexing_status_.has_value()) {
      session.transfer(ff_status.flow_file_, QuerySplunkIndexingStatus::Failure);
    } else {
      if (ff_status.indexing_status_.value()) {
        session.transfer(ff_status.flow_file_, QuerySplunkIndexingStatus::Acknowledged);
      } else if (flowFileAcknowledgementTimedOut(ff_status.flow_file_, max_age)) {
        session.transfer(ff_status.flow_file_, QuerySplunkIndexingStatus::Unacknowledged);
      } else {
        session.penalize(ff_status.flow_file_);
        session.transfer(ff_status.flow_file_, QuerySplunkIndexingStatus::Undetermined);
      }
    }
  }
}
}  // namespace

void QuerySplunkIndexingStatus::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session);
  std::string ack_request;

  utils::HTTPClient client;
  initializeClient(client, getNetworkLocation().append(getEndpoint()), getSSLContextService(*context));
  auto undetermined_flow_files = getUndeterminedFlowFiles(*session, batch_size_);
  if (undetermined_flow_files.empty())
    return;
  client.setPostFields(getAckIdsAsPayload(undetermined_flow_files));
  getIndexingStatusFromSplunk(client, undetermined_flow_files);
  routeFlowFilesBasedOnIndexingStatus(*session, undetermined_flow_files, max_age_);
}


REGISTER_RESOURCE(QuerySplunkIndexingStatus, "Queries Splunk server in order to acquire the status of indexing acknowledgement.");

}  // namespace org::apache::nifi::minifi::extensions::splunk
