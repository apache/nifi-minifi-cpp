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

#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

void WriteToFlowFileTestProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void WriteToFlowFileTestProcessor::onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) {
  logger_->log_info("{}", ON_SCHEDULE_LOG_STR);
}

void WriteToFlowFileTestProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_info("{}", ON_TRIGGER_LOG_STR);
  if (content_.empty()) {
    context.yield();
    return;
  }
  std::shared_ptr<core::FlowFile> flow_file = session.create();
  if (!flow_file) {
    logger_->log_error("Failed to create flowfile!");
    return;
  }
  session.writeBuffer(flow_file, content_);
  session.transfer(flow_file, Success);
}

void WriteToFlowFileTestProcessor::onUnSchedule() {
  logger_->log_info("{}", ON_UNSCHEDULE_LOG_STR);
}

REGISTER_RESOURCE(WriteToFlowFileTestProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
