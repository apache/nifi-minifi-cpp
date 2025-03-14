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
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "http/HTTPClient.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::extensions::splunk {

namespace {
constexpr std::string_view getEndpoint() {
  return "/services/collector/ack";
}

struct FlowFileWithIndexStatus {
  explicit FlowFileWithIndexStatus(gsl::not_null<std::shared_ptr<core::FlowFile>>&& flow_file) : flow_file_(std::move(flow_file)) {}

  gsl::not_null<std::shared_ptr<core::FlowFile>> flow_file_;
  std::optional<bool> indexing_status_ = std::nullopt;
};

std::unordered_map<uint64_t, FlowFileWithIndexStatus> getUndeterminedFlowFiles(core::ProcessSession& session, uint64_t batch_size) {
  std::unordered_map<uint64_t, FlowFileWithIndexStatus> undetermined_flow_files;
  std::unordered_set<uint64_t> duplicate_ack_ids;
  for (uint64_t i = 0; i < batch_size; ++i) {
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

void getIndexingStatusFromSplunk(http::HTTPClient& client, std::unordered_map<uint64_t, FlowFileWithIndexStatus>& undetermined_flow_files) {
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
  return system_clock::now() > std::chrono::system_clock::time_point() + std::chrono::milliseconds(splunk_response_time) + max_age;
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

void QuerySplunkIndexingStatus::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void QuerySplunkIndexingStatus::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  SplunkHECProcessor::onSchedule(context, session_factory);
  max_age_ = utils::parseDurationProperty(context, MaximumWaitingTime);
  batch_size_ = utils::parseU64Property(context, MaxQuerySize);
  initializeClient(client_, getNetworkLocation().append(getEndpoint()), getSSLContextService(context));
}

void QuerySplunkIndexingStatus::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  std::string ack_request;

  auto undetermined_flow_files = getUndeterminedFlowFiles(session, batch_size_);
  if (undetermined_flow_files.empty())
    return;
  client_.setPostFields(getAckIdsAsPayload(undetermined_flow_files));
  getIndexingStatusFromSplunk(client_, undetermined_flow_files);
  routeFlowFilesBasedOnIndexingStatus(session, undetermined_flow_files, max_age_);
}

REGISTER_RESOURCE(QuerySplunkIndexingStatus, Processor);

}  // namespace org::apache::nifi::minifi::extensions::splunk
