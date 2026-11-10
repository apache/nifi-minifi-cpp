// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "ReadFromFlowFileTestProcessor.h"

#include <utility>
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

void ReadFromFlowFileTestProcessor::initialize() {
  setSupportedRelationships(Relationships);
}

void ReadFromFlowFileTestProcessor::onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) {
  logger_->log_info("{}", ON_SCHEDULE_LOG_STR);
}

void ReadFromFlowFileTestProcessor::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  logger_->log_info("{}", ON_TRIGGER_LOG_STR);
  if (clear_on_trigger_)
    clear();

  while (std::shared_ptr<core::FlowFile> flow_file = session.get()) {
    session.transfer(flow_file, Success);
    flow_files_read_.emplace_back(session, gsl::not_null(std::move(flow_file)));
  }
}

void ReadFromFlowFileTestProcessor::onUnSchedule() {
  logger_->log_info("{}", ON_UNSCHEDULE_LOG_STR);
}

ReadFromFlowFileTestProcessor::FlowFileData::FlowFileData(core::ProcessSession& session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file) {
  content_ = to_string(session.readBuffer(flow_file));
  attributes_ = flow_file->getAttributes();
}

bool ReadFromFlowFileTestProcessor::readFlowFileWithContent(const std::string& content) const {
  return std::find_if(flow_files_read_.begin(), flow_files_read_.end(), [&content](const FlowFileData& flow_file_data){ return flow_file_data.content_ == content; }) != flow_files_read_.end();
}

bool ReadFromFlowFileTestProcessor::readFlowFileWithAttribute(const std::string& key) const {
  return std::find_if(flow_files_read_.begin(),
      flow_files_read_.end(),
      [&key](const FlowFileData& flow_file_data) { return flow_file_data.attributes_.contains(key); }) != flow_files_read_.end();
}

bool ReadFromFlowFileTestProcessor::readFlowFileWithAttribute(const std::string& key, const std::string& value) const {
  return std::find_if(flow_files_read_.begin(),
      flow_files_read_.end(),
      [&key, &value](const FlowFileData& flow_file_data) { return flow_file_data.attributes_.contains(key) && flow_file_data.attributes_.at(key) == value; }) != flow_files_read_.end();
}

REGISTER_RESOURCE(ReadFromFlowFileTestProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
