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
#include "PushGrafanaLokiREST.h"

#include <utility>
#include <fstream>
#include <filesystem>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

void PushGrafanaLokiREST::LogBatch::add(const std::shared_ptr<core::FlowFile>& flowfile) {
  gsl_Expects(state_manager_);
  if (log_line_batch_wait_ && batched_flowfiles_.empty()) {
    start_push_time_ = std::chrono::steady_clock::now();
    std::unordered_map<std::string, std::string> state;
    state["start_push_time"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(start_push_time_.time_since_epoch()).count());
    logger_->log_debug("Saved start push time to state: {}", state["start_push_time"]);
    state_manager_->set(state);
  }
  batched_flowfiles_.push_back(flowfile);
}

void PushGrafanaLokiREST::LogBatch::restore(const std::shared_ptr<core::FlowFile>& flowfile) {
  batched_flowfiles_.push_back(flowfile);
}

std::vector<std::shared_ptr<core::FlowFile>> PushGrafanaLokiREST::LogBatch::flush() {
  gsl_Expects(state_manager_);
  start_push_time_ = {};
  auto result = batched_flowfiles_;
  batched_flowfiles_.clear();
  if (log_line_batch_wait_) {
    start_push_time_ = {};
    std::unordered_map<std::string, std::string> state;
    logger_->log_debug("Reset start push time state");
    state["start_push_time"] = "0";
    state_manager_->set(state);
  }
  return result;
}

bool PushGrafanaLokiREST::LogBatch::isReady() const {
  return (log_line_batch_size_ && batched_flowfiles_.size() >= *log_line_batch_size_) || (log_line_batch_wait_ && std::chrono::steady_clock::now() - start_push_time_ >= *log_line_batch_wait_);
}

void PushGrafanaLokiREST::LogBatch::setLogLineBatchSize(std::optional<uint64_t> log_line_batch_size) {
  log_line_batch_size_ = log_line_batch_size;
}

void PushGrafanaLokiREST::LogBatch::setLogLineBatchWait(std::optional<std::chrono::milliseconds> log_line_batch_wait) {
  log_line_batch_wait_ = log_line_batch_wait;
}

void PushGrafanaLokiREST::LogBatch::setStateManager(core::StateManager* state_manager) {
  state_manager_ = state_manager;
}

void PushGrafanaLokiREST::LogBatch::setStartPushTime(std::chrono::steady_clock::time_point start_push_time) {
  start_push_time_ = start_push_time;
}

const core::Relationship PushGrafanaLokiREST::Self("__self__", "Marks the FlowFile to be owned by this processor");

void PushGrafanaLokiREST::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

namespace {
auto getSSLContextService(core::ProcessContext& context) {
  if (auto ssl_context = context.getProperty(PushGrafanaLokiREST::SSLContextService)) {
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(*ssl_context));
  }
  return std::shared_ptr<minifi::controllers::SSLContextService>{};
}

std::string readLogLineFromFlowFile(const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) {
  auto read_buffer_result = session.readBuffer(flow_file);
  return {reinterpret_cast<const char*>(read_buffer_result.buffer.data()), read_buffer_result.buffer.size()};
}
}  // namespace

void PushGrafanaLokiREST::setUpStateManager(core::ProcessContext& context) {
  auto state_manager = context.getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  log_batch_.setStateManager(state_manager);

  std::unordered_map<std::string, std::string> state_map;
  if (state_manager->get(state_map)) {
    auto it = state_map.find("start_push_time");
    if (it != state_map.end()) {
      logger_->log_info("Restored start push time from processor state: {}", it->second);
      std::chrono::steady_clock::time_point start_push_time{std::chrono::milliseconds{std::stoll(it->second)}};
      log_batch_.setStartPushTime(start_push_time);
    }
  }
}

void PushGrafanaLokiREST::setUpStreamLabels(core::ProcessContext& context) {
  if (auto stream_labels_str = context.getProperty(StreamLabels)) {
    auto stream_labels = utils::StringUtils::splitAndTrimRemovingEmpty(*stream_labels_str, ",");
    if (stream_labels.empty()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Label Attributes");
    }
    for (const auto& label : stream_labels) {
      auto stream_labels = utils::StringUtils::splitAndTrimRemovingEmpty(label, "=");
      if (stream_labels.size() != 2) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Label Attributes");
      }
      stream_label_attributes_[stream_labels[0]] = stream_labels[1];
    }
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Label Attributes");
  }
}

void PushGrafanaLokiREST::setupClientTimeouts(const core::ProcessContext& context) {
  if (auto connection_timeout = context.getProperty<core::TimePeriodValue>(PushGrafanaLokiREST::ConnectTimeout)) {
    client_.setConnectionTimeout(connection_timeout->getMilliseconds());
  }

  if (auto read_timeout = context.getProperty<core::TimePeriodValue>(PushGrafanaLokiREST::ReadTimeout)) {
    client_.setReadTimeout(read_timeout->getMilliseconds());
  }
}

void PushGrafanaLokiREST::setAuthorization(const core::ProcessContext& context) {
  if (auto username = context.getProperty(PushGrafanaLokiREST::Username)) {
    auto password = context.getProperty(PushGrafanaLokiREST::Password);
    if (!password) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Username is set, but Password property is not!");
    }
    std::string auth = *username + ":" + *password;
    auto base64_encoded_auth = utils::StringUtils::to_base64(auth);
    client_.setRequestHeader("Authorization", "Basic " + base64_encoded_auth);
  } else if (auto bearer_token_file = context.getProperty(PushGrafanaLokiREST::BearerTokenFile)) {
    if (!std::filesystem::exists(*bearer_token_file) || !std::filesystem::is_regular_file(*bearer_token_file)) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bearer Token File is not a regular file!");
    }
    std::ifstream file(*bearer_token_file);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string bearer_token = utils::StringUtils::trim(buffer.str());
    client_.setRequestHeader("Authorization", "Bearer " + bearer_token);
  } else {
    client_.setRequestHeader("Authorization", std::nullopt);
  }
}

void PushGrafanaLokiREST::initializeHttpClient(core::ProcessContext& context) {
  auto url = utils::getRequiredPropertyOrThrow<std::string>(context, Url.name);
  if (url.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Url property cannot be empty!");
  }
  if (utils::StringUtils::endsWith(url, "/")) {
    url += "loki/api/v1/push";
  } else {
    url += "/loki/api/v1/push";
  }
  logger_->log_debug("PushGrafanaLokiREST push url is set to: {}", url);
  client_.initialize(utils::HttpRequestMethod::POST, url, getSSLContextService(context));
}

void PushGrafanaLokiREST::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  setUpStateManager(context);
  initializeHttpClient(context);
  client_.setContentType("application/json");
  client_.setFollowRedirects(true);

  setUpStreamLabels(context);

  if (auto log_line_metadata_attributes = context.getProperty(LogLineMetadataAttributes)) {
    log_line_metadata_attributes_ = utils::StringUtils::splitAndTrimRemovingEmpty(*log_line_metadata_attributes, ",");
  }

  auto tenant_id = context.getProperty(TenantID);
  if (tenant_id && !tenant_id->empty()) {
    client_.setRequestHeader("X-Scope-OrgID", tenant_id);
  } else {
    client_.setRequestHeader("X-Scope-OrgID", std::nullopt);
  }
  auto log_line_batch_wait = context.getProperty<core::TimePeriodValue>(LogLineBatchWait);
  auto log_line_batch_size = context.getProperty<uint64_t>(LogLineBatchSize);
    if (log_line_batch_size && *log_line_batch_size < 1) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Log Line Batch Size property is invalid!");
  }
  log_line_batch_size_is_set_ = log_line_batch_size.has_value();
  log_line_batch_wait_is_set_ = log_line_batch_wait.has_value();

  max_batch_size_ = context.getProperty<uint64_t>(MaxBatchSize);
  if (max_batch_size_) {
    logger_->log_debug("PushGrafanaLokiREST Max Batch Size is set to: {}", *max_batch_size_);
  }

  log_batch_.setLogLineBatchSize(log_line_batch_size);
  if (log_line_batch_size) {
    logger_->log_debug("PushGrafanaLokiREST Log Line Batch Size is set to: {}", *log_line_batch_size);
  }

  if (log_line_batch_wait) {
    log_batch_.setLogLineBatchWait(log_line_batch_wait->getMilliseconds());
    logger_->log_debug("PushGrafanaLokiREST Log Line Batch Wait is set to {} milliseconds", log_line_batch_wait->getMilliseconds());
  }

  setupClientTimeouts(context);
  setAuthorization(context);
}

void PushGrafanaLokiREST::addLogLineMetadata(rapidjson::Value& log_line, rapidjson::Document::AllocatorType& allocator, core::FlowFile& flow_file) const {
  if (log_line_metadata_attributes_.empty()) {
    return;
  }

  rapidjson::Value labels(rapidjson::kObjectType);
  for (const auto& label : log_line_metadata_attributes_) {
    auto attribute_value = flow_file.getAttribute(label);
    if (!attribute_value) {
      continue;
    }
    rapidjson::Value label_key(label.c_str(), gsl::narrow<rapidjson::SizeType>(label.length()), allocator);
    rapidjson::Value label_value(attribute_value->c_str(), gsl::narrow<rapidjson::SizeType>(attribute_value->length()), allocator);
    labels.AddMember(label_key, label_value, allocator);
  }
  log_line.PushBack(labels, allocator);
}

std::string PushGrafanaLokiREST::createLokiJson(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) const {
  rapidjson::Document document;
  document.SetObject();
  rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

  rapidjson::Value streams(rapidjson::kArrayType);
  rapidjson::Value stream_object(rapidjson::kObjectType);

  rapidjson::Value labels(rapidjson::kObjectType);
  for (const auto& [key, value] : stream_label_attributes_) {
    rapidjson::Value label(key.c_str(), allocator);
    labels.AddMember(label, rapidjson::Value(value.c_str(), allocator), allocator);
  }

  rapidjson::Value values(rapidjson::kArrayType);
  for (const auto& flow_file : batched_flow_files) {
    rapidjson::Value log_line(rapidjson::kArrayType);

    auto timestamp_str = std::to_string(flow_file->getlineageStartDate().time_since_epoch() / std::chrono::nanoseconds(1));
    rapidjson::Value timestamp;
    timestamp.SetString(timestamp_str.c_str(), gsl::narrow<rapidjson::SizeType>(timestamp_str.length()), allocator);
    rapidjson::Value log_line_value;

    auto line = readLogLineFromFlowFile(flow_file, session);
    log_line_value.SetString(line.c_str(), gsl::narrow<rapidjson::SizeType>(line.length()), allocator);

    log_line.PushBack(timestamp, allocator);
    log_line.PushBack(log_line_value, allocator);
    addLogLineMetadata(log_line, allocator, *flow_file);
    values.PushBack(log_line, allocator);
  }

  stream_object.AddMember("stream", labels, allocator);
  stream_object.AddMember("values", values, allocator);
  streams.PushBack(stream_object, allocator);
  document.AddMember("streams", streams, allocator);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);
  return buffer.GetString();
}

nonstd::expected<void, std::string> PushGrafanaLokiREST::submitRequest(const std::string& loki_json) {
  client_.setPostFields(loki_json);
  if (!client_.submit()) {
    return nonstd::make_unexpected("Submit failed");
  }
  auto response_code = client_.getResponseCode();
  if (response_code < 200 || response_code >= 300) {
    return nonstd::make_unexpected("Error occurred: " + std::to_string(response_code));
  }
  return {};
}

void PushGrafanaLokiREST::processBatch(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) {
  if (batched_flow_files.empty()) {
    return;
  }

  auto loki_json = createLokiJson(batched_flow_files, session);
  auto result = submitRequest(loki_json);
  if (!result) {
    logger_->log_error("Failed to send log batch to Loki: {}", result.error());
    for (const auto& flow_file : batched_flow_files) {
      session.transfer(flow_file, Failure);
    }
  } else {
    logger_->log_debug("Successfully sent log batch with {} log lines to Loki", batched_flow_files.size());
    for (const auto& flow_file : batched_flow_files) {
      session.transfer(flow_file, Success);
    }
  }
}

void PushGrafanaLokiREST::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  uint64_t flow_files_read = 0;
  std::vector<std::shared_ptr<core::FlowFile>> to_be_transferred_flow_files;
  while (!max_batch_size_ || *max_batch_size_ == 0 || flow_files_read < *max_batch_size_) {
    std::shared_ptr<core::FlowFile> flow_file = session.get();
    if (!flow_file) {
      break;
    }

    to_be_transferred_flow_files.push_back(flow_file);
    logger_->log_debug("Enqueuing flow file {} to be sent to Loki", flow_file->getUUIDStr());
    log_batch_.add(flow_file);
    if (log_batch_.isReady()) {  // if no log line batch limit is set, then log batch will never be ready
      auto batched_flow_files = log_batch_.flush();
      for (const auto& flow : batched_flow_files) {
        if (to_be_transferred_flow_files[0] == flow) {  // we don't want to add the flowfiles that are already in this session
          break;
        }
        session.add(flow);
      }
      logger_->log_debug("Sending {} log lines to Loki", batched_flow_files.size());
      processBatch(batched_flow_files, session);
      to_be_transferred_flow_files.clear();
    }

    ++flow_files_read;
  }

  if (!log_line_batch_size_is_set_ && !log_line_batch_wait_is_set_) {  // if no log line batch limit is set, then the log batch will contain all the flow files in the trigger that should be sent
    auto batched_flow_files = log_batch_.flush();
    logger_->log_debug("Sending {} log lines to Loki", batched_flow_files.size());
    processBatch(batched_flow_files, session);
  } else if (flow_files_read == 0 && log_line_batch_wait_is_set_) {  // if no flow files were read, but wait time is set for log batch, we should see if it is ready to be sent
    if (!log_batch_.isReady()) {
      return;
    }
    auto batched_flow_files = log_batch_.flush();
    for (const auto& flow : batched_flow_files) {
      session.add(flow);
    }
    logger_->log_debug("Sending {} log lines to Loki", batched_flow_files.size());
    processBatch(batched_flow_files, session);
  } else if (flow_files_read == 0) {  // if no flow files were read and no log batch wait time is set, then we should yield
    context.yield();
  } else {  // if flow files were read, then assume ownership of incoming, non-transferred flow files
    for (const auto& flow_file : to_be_transferred_flow_files) {
      session.transfer(flow_file, Self);
    }
  }
}

void PushGrafanaLokiREST::restore(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!flow_file) {
    return;
  }
  logger_->log_debug("Restoring flow file {} from flow file repository", flow_file->getUUIDStr());
  log_batch_.restore(flow_file);
}

std::set<core::Connectable*> PushGrafanaLokiREST::getOutGoingConnections(const std::string &relationship) {
  auto result = core::Connectable::getOutGoingConnections(relationship);
  if (relationship == Self.getName()) {
    result.insert(this);
  }
  return result;
}

REGISTER_RESOURCE(PushGrafanaLokiREST, Processor);

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
