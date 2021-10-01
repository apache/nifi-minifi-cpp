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

#include "WriteToFlowFileTestProcessor.h"

#include <string>
#include <vector>

namespace org::apache::nifi::minifi::processors {

const std::string WriteToFlowFileTestProcessor::OnScheduleLogStr = "WriteToFlowFileTestProcessor::onSchedule executed";
const std::string WriteToFlowFileTestProcessor::OnTriggerLogStr = "WriteToFlowFileTestProcessor::onTrigger executed";
const std::string WriteToFlowFileTestProcessor::OnUnScheduleLogStr = "WriteToFlowFileTestProcessor::onUnSchedule";

core::Relationship WriteToFlowFileTestProcessor::Success("success", "success operational on the flow record");


void WriteToFlowFileTestProcessor::initialize() {
  setSupportedProperties({});
  setSupportedRelationships({Success});
}

void WriteToFlowFileTestProcessor::onSchedule(core::ProcessContext*, core::ProcessSessionFactory*) {
  logger_->log_info("%s", OnScheduleLogStr);
}

namespace {
struct WriteBufferToFlowFile : public OutputStreamCallback {
  const std::vector<uint8_t> buffer_;

  explicit WriteBufferToFlowFile(const std::string& buffer) : buffer_(buffer.begin(), buffer.end()) {}

  int64_t process(const std::shared_ptr<io::BaseStream> &stream) override {
    size_t bytes_written = stream->write(buffer_, buffer_.size());
    return io::isError(bytes_written) ? -1 : gsl::narrow<int64_t>(bytes_written);
  }
};
}  // namespace

void WriteToFlowFileTestProcessor::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  logger_->log_info("%s", OnTriggerLogStr);
  if (content_.empty()) {
    context->yield();
    return;
  }
  std::shared_ptr<core::FlowFile> flow_file = session->create();
  if (!flow_file) {
    logger_->log_error("Failed to create flowfile!");
    return;
  }
  WriteBufferToFlowFile callback(content_);
  session->write(flow_file, &callback);
  session->transfer(flow_file, Success);
}

void WriteToFlowFileTestProcessor::onUnSchedule() {
  logger_->log_info("%s", OnUnScheduleLogStr);
}

}  // namespace org::apache::nifi::minifi::processors
