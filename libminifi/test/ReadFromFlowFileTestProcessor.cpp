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
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }
  ReadFlowFileIntoBuffer callback;
  session->read(flow_file, &callback);
  content_ = std::string(callback.buffer_.begin(), callback.buffer_.end());
  session->transfer(flow_file, Success);
}

void ReadFromFlowFileTestProcessor::onUnSchedule() {
  logger_->log_info("%s", ON_UNSCHEDULE_LOG_STR);
}

REGISTER_RESOURCE(ReadFromFlowFileTestProcessor, "ReadFromFlowFileTestProcessor (only for testing purposes)");

}  // namespace org::apache::nifi::minifi::processors
