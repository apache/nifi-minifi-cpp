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
#include "PushGrafanaLoki.h"

#include <utility>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

void PushGrafanaLoki::LogBatch::add(const std::shared_ptr<core::FlowFile>& flowfile) {
  gsl_Expects(state_manager_);
  if (log_line_batch_wait_ && batched_flowfiles_.empty()) {
    start_push_time_ = std::chrono::system_clock::now();
    std::unordered_map<std::string, std::string> state;
    state["start_push_time"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(start_push_time_.time_since_epoch()).count());
    logger_->log_debug("Saved start push time to state: {}", state["start_push_time"]);
    state_manager_->set(state);
  }
  batched_flowfiles_.push_back(flowfile);
}

void PushGrafanaLoki::LogBatch::restore(const std::shared_ptr<core::FlowFile>& flowfile) {
  batched_flowfiles_.push_back(flowfile);
}

std::vector<std::shared_ptr<core::FlowFile>> PushGrafanaLoki::LogBatch::flush() {
  gsl_Expects(state_manager_);
  start_push_time_ = {};
  auto result = batched_flowfiles_;
  batched_flowfiles_.clear();
  if (log_line_batch_wait_) {
    std::unordered_map<std::string, std::string> state;
    logger_->log_debug("Reset start push time state");
    state["start_push_time"] = "0";
    state_manager_->set(state);
  }
  return result;
}

bool PushGrafanaLoki::LogBatch::isReady() const {
  return (log_line_batch_size_ && batched_flowfiles_.size() >= *log_line_batch_size_) || (log_line_batch_wait_ && std::chrono::system_clock::now() - start_push_time_ >= *log_line_batch_wait_);
}

void PushGrafanaLoki::LogBatch::setLogLineBatchSize(std::optional<uint64_t> log_line_batch_size) {
  log_line_batch_size_ = log_line_batch_size;
}

void PushGrafanaLoki::LogBatch::setLogLineBatchWait(std::optional<std::chrono::milliseconds> log_line_batch_wait) {
  log_line_batch_wait_ = log_line_batch_wait;
}

void PushGrafanaLoki::LogBatch::setStateManager(core::StateManager* state_manager) {
  state_manager_ = state_manager;
}

void PushGrafanaLoki::LogBatch::setStartPushTime(std::chrono::system_clock::time_point start_push_time) {
  start_push_time_ = start_push_time;
}

const core::Relationship PushGrafanaLoki::Self("__self__", "Marks the FlowFile to be owned by this processor");

std::shared_ptr<minifi::controllers::SSLContextService> PushGrafanaLoki::getSSLContextService(core::ProcessContext& context) {
  if (auto ssl_context = context.getProperty(PushGrafanaLoki::SSLContextService)) {
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(*ssl_context));
  }
  return std::shared_ptr<minifi::controllers::SSLContextService>{};
}

void PushGrafanaLoki::setUpStateManager(core::ProcessContext& context) {
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
      std::chrono::system_clock::time_point start_push_time{std::chrono::milliseconds{std::stoll(it->second)}};
      log_batch_.setStartPushTime(start_push_time);
    }
  }
}

std::map<std::string, std::string> PushGrafanaLoki::buildStreamLabelMap(core::ProcessContext& context) {
  std::map<std::string, std::string> stream_label_map;
  if (auto stream_labels_str = context.getProperty(StreamLabels)) {
    auto stream_labels = utils::string::splitAndTrimRemovingEmpty(*stream_labels_str, ",");
    if (stream_labels.empty()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Labels property");
    }
    for (const auto& label : stream_labels) {
      auto stream_labels = utils::string::splitAndTrimRemovingEmpty(label, "=");
      if (stream_labels.size() != 2) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Labels property");
      }
      stream_label_map[stream_labels[0]] = stream_labels[1];
    }
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Labels property");
  }
  return stream_label_map;
}

void PushGrafanaLoki::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  setUpStateManager(context);
  setUpStreamLabels(context);

  if (auto log_line_metadata_attributes = context.getProperty(LogLineMetadataAttributes)) {
    log_line_metadata_attributes_ = utils::string::splitAndTrimRemovingEmpty(*log_line_metadata_attributes, ",");
  }

  auto log_line_batch_wait = context.getProperty<core::TimePeriodValue>(LogLineBatchWait);
  auto log_line_batch_size = context.getProperty<uint64_t>(LogLineBatchSize);
  if (log_line_batch_size && *log_line_batch_size < 1) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Log Line Batch Size property is missing or less than 1!");
  }
  log_line_batch_size_is_set_ = log_line_batch_size.has_value();
  log_line_batch_wait_is_set_ = log_line_batch_wait.has_value();

  max_batch_size_ = context.getProperty<uint64_t>(MaxBatchSize);
  if (max_batch_size_) {
    logger_->log_debug("PushGrafanaLoki Max Batch Size is set to: {}", *max_batch_size_);
  }

  log_batch_.setLogLineBatchSize(log_line_batch_size);
  if (log_line_batch_size) {
    logger_->log_debug("PushGrafanaLoki Log Line Batch Size is set to: {}", *log_line_batch_size);
  }

  if (log_line_batch_wait) {
    log_batch_.setLogLineBatchWait(log_line_batch_wait->getMilliseconds());
    logger_->log_debug("PushGrafanaLoki Log Line Batch Wait is set to {} milliseconds", log_line_batch_wait->getMilliseconds());
  }
}

void PushGrafanaLoki::processBatch(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) {
  if (batched_flow_files.empty()) {
    return;
  }

  auto result = submitRequest(batched_flow_files, session);
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

void PushGrafanaLoki::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
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

void PushGrafanaLoki::restore(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!flow_file) {
    return;
  }
  logger_->log_debug("Restoring flow file {} from flow file repository", flow_file->getUUIDStr());
  log_batch_.restore(flow_file);
}

std::set<core::Connectable*> PushGrafanaLoki::getOutGoingConnections(const std::string &relationship) {
  auto result = core::ConnectableImpl::getOutGoingConnections(relationship);
  if (relationship == Self.getName()) {
    result.insert(this);
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
