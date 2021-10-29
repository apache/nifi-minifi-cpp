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
#include "ReadFromFlowFileTestProcessor.h"

#include <string>
#include <utility>
#include <vector>

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

const core::Relationship ReadFromFlowFileTestProcessor::Success("success", "success operational on the flow record");

void ReadFromFlowFileTestProcessor::initialize() {
  setSupportedRelationships({ Success });
}

void ReadFromFlowFileTestProcessor::onSchedule(core::ProcessContext*, core::ProcessSessionFactory*) {
  logger_->log_info("%s", ON_SCHEDULE_LOG_STR);
}

namespace {
struct ReadFlowFileIntoBuffer : public InputStreamCallback {
  std::vector<uint8_t> buffer_;

  int64_t process(const std::shared_ptr<io::BaseStream> &stream) override {
    size_t bytes_read = stream->read(buffer_, stream->size());
    return io::isError(bytes_read) ? -1 : gsl::narrow<int64_t>(bytes_read);
  }
};
}

void ReadFromFlowFileTestProcessor::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  gsl_Expects(context && session);
  logger_->log_info("%s", ON_TRIGGER_LOG_STR);
  flow_files_read_.clear();

  while (std::shared_ptr<core::FlowFile> flow_file = session->get()) {
    ReadFlowFileIntoBuffer callback;
    session->read(flow_file, &callback);
    session->transfer(flow_file, Success);
    flow_files_read_.emplace_back(session, gsl::not_null(std::move(flow_file)));
  }
}

void ReadFromFlowFileTestProcessor::onUnSchedule() {
  logger_->log_info("%s", ON_UNSCHEDULE_LOG_STR);
}

ReadFromFlowFileTestProcessor::FlowFileData::FlowFileData(core::ProcessSession* session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file) {
  ReadFlowFileIntoBuffer callback;
  session->read(flow_file, &callback);
  content_ = std::string(callback.buffer_.begin(), callback.buffer_.end());
  attributes_ = flow_file->getAttributes();
}

bool ReadFromFlowFileTestProcessor::readFlowFileWithContent(const std::string& content) const {
  return std::find_if(flow_files_read_.begin(), flow_files_read_.end(), [&content](FlowFileData flow_file_data){ return flow_file_data.content_ == content; }) != flow_files_read_.end();
}

bool ReadFromFlowFileTestProcessor::readFlowFileWithAttribute(const std::string& key) const {
  return std::find_if(flow_files_read_.begin(),
                      flow_files_read_.end(),
                      [&key](FlowFileData flow_file_data) { return flow_file_data.attributes_.contains(key); }) != flow_files_read_.end();
}

bool ReadFromFlowFileTestProcessor::readFlowFileWithAttribute(const std::string& key, const std::string& value) const {
  return std::find_if(flow_files_read_.begin(),
                      flow_files_read_.end(),
                      [&key, &value](FlowFileData flow_file_data) { return flow_file_data.attributes_.contains(key) && flow_file_data.attributes_.at(key) == value; }) != flow_files_read_.end();
}


REGISTER_RESOURCE(ReadFromFlowFileTestProcessor, "ReadFromFlowFileTestProcessor (only for testing purposes)");

}  // namespace org::apache::nifi::minifi::processors
