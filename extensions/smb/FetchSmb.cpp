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

#include "FetchSmb.h"
#include "core/Resource.h"
#include "utils/ConfigurationUtils.h"
#include "utils/file/FileReaderCallback.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::extensions::smb {

void FetchSmb::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchSmb::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  smb_connection_controller_service_ = utils::parseControllerService<SmbConnectionControllerService>(context, FetchSmb::ConnectionControllerService, getUUID());
  buffer_size_ = utils::configuration::getBufferSize(*context.getConfiguration());
}

namespace {
std::filesystem::path getTargetRelativePath(core::ProcessContext& context, const core::FlowFile& flow_file) {
  auto remote_file = context.getProperty(FetchSmb::RemoteFile, &flow_file);
  if (remote_file && !remote_file->empty()) {
    return std::filesystem::path{*remote_file}.relative_path();  // We need to make sure that the path remains relative (e.g. ${path}/foo where ${path} is empty can lead to /foo)
  }
  std::filesystem::path path = flow_file.getAttribute(core::SpecialFlowAttribute::PATH).value_or("");
  std::filesystem::path filename = flow_file.getAttribute(core::SpecialFlowAttribute::FILENAME).value_or("");
  return path / filename;
}
}  // namespace

void FetchSmb::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(smb_connection_controller_service_);

  if (auto connection_error = smb_connection_controller_service_->validateConnection()) {
    logger_->log_error("Couldn't establish connection to the specified network location due to {}", connection_error.message());
    context.yield();
    return;
  }

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  try {
    session.write(flow_file, utils::FileReaderCallback{smb_connection_controller_service_->getPath() / getTargetRelativePath(context, *flow_file), buffer_size_});
    session.transfer(flow_file, Success);
  } catch (const utils::FileReaderCallbackIOError& io_error) {
    flow_file->addAttribute(ErrorCode.name, std::to_string(io_error.error_code));
    flow_file->addAttribute(ErrorMessage.name, io_error.what());
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(FetchSmb, Processor);

}  // namespace org::apache::nifi::minifi::extensions::smb
