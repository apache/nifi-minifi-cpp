/**
 *
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

#include "core/FlowConfiguration.h"
#include <memory>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

FlowConfiguration::~FlowConfiguration() {
}

std::shared_ptr<core::Processor> FlowConfiguration::createProcessor(
    std::string name, uuid_t uuid) {
  std::shared_ptr<core::Processor> processor = nullptr;
  if (name
      == org::apache::nifi::minifi::processors::GenerateFlowFile::ProcessorName) {
    processor = std::make_shared<
        org::apache::nifi::minifi::processors::GenerateFlowFile>(name, uuid);
  } else if (name
      == org::apache::nifi::minifi::processors::LogAttribute::ProcessorName) {
    processor = std::make_shared<
        org::apache::nifi::minifi::processors::LogAttribute>(name, uuid);
  } else if (name
      == org::apache::nifi::minifi::processors::GetFile::ProcessorName) {
    processor =
        std::make_shared<org::apache::nifi::minifi::processors::GetFile>(name,
                                                                         uuid);
  } else if (name
      == org::apache::nifi::minifi::processors::PutFile::ProcessorName) {
    processor =
        std::make_shared<org::apache::nifi::minifi::processors::PutFile>(name,
                                                                         uuid);
  } else if (name
      == org::apache::nifi::minifi::processors::TailFile::ProcessorName) {
    processor =
        std::make_shared<org::apache::nifi::minifi::processors::TailFile>(name,
                                                                          uuid);
  } else if (name
      == org::apache::nifi::minifi::processors::ListenSyslog::ProcessorName) {
    processor = std::make_shared<
        org::apache::nifi::minifi::processors::ListenSyslog>(name, uuid);
  } else if (name
        == org::apache::nifi::minifi::processors::ListenHTTP::ProcessorName) {
      processor = std::make_shared<
          org::apache::nifi::minifi::processors::ListenHTTP>(name, uuid);
  } else if (name
          == org::apache::nifi::minifi::processors::InvokeHTTP::ProcessorName) {
        processor = std::make_shared<
            org::apache::nifi::minifi::processors::InvokeHTTP>(name, uuid);
  } else if (name
      == org::apache::nifi::minifi::processors::ExecuteProcess::ProcessorName) {
    processor = std::make_shared<
        org::apache::nifi::minifi::processors::ExecuteProcess>(name, uuid);
  } else if (name
      == org::apache::nifi::minifi::processors::AppendHostInfo::ProcessorName) {
    processor = std::make_shared<
        org::apache::nifi::minifi::processors::AppendHostInfo>(name, uuid);
  } else {
    logger_->log_error("No Processor defined for %s", name.c_str());
    return nullptr;
  }

  // initialize the processor
  processor->initialize();

  return processor;
}

std::shared_ptr<core::Processor> FlowConfiguration::createProvenanceReportTask() {
  std::shared_ptr<core::Processor> processor = nullptr;

  processor = std::make_shared<
        org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(stream_factory_);
  // initialize the processor
  processor->initialize();

  return processor;
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createRootProcessGroup(
    std::string name, uuid_t uuid) {
  return std::unique_ptr<core::ProcessGroup>(
      new core::ProcessGroup(core::ROOT_PROCESS_GROUP, name, uuid));
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createRemoteProcessGroup(
    std::string name, uuid_t uuid) {
  return std::unique_ptr<core::ProcessGroup>(
      new core::ProcessGroup(core::REMOTE_PROCESS_GROUP, name, uuid));
}

std::shared_ptr<minifi::Connection> FlowConfiguration::createConnection(
    std::string name, uuid_t uuid) {
  return std::make_shared<minifi::Connection>(flow_file_repo_, name, uuid);
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
