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
#ifndef LIBMINIFI_INCLUDE_CORE_FLOWCONFIGURATION_H_
#define LIBMINIFI_INCLUDE_CORE_FLOWCONFIGURATION_H_

#include "core/Core.h"
#include "Connection.h"
#include "RemoteProcessorGroupPort.h"
#include "core/controller/ControllerServiceNode.h"
#include "core/controller/StandardControllerServiceProvider.h"
#include "provenance/Provenance.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/TailFile.h"
#include "processors/ListenSyslog.h"
#include "processors/GenerateFlowFile.h"
#include "processors/InvokeHTTP.h"
#include "processors/ListenHTTP.h"
#include "processors/LogAttribute.h"
#include "processors/ExecuteProcess.h"
#include "processors/AppendHostInfo.h"
#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessGroup.h"
#include "io/StreamFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Purpose: Flow configuration defines the mechanism
 * by which we will configure our flow controller
 */
class FlowConfiguration : public CoreComponent {
 public:
  /**
   * Constructor that will be used for configuring
   * the flow controller.
   */
  explicit FlowConfiguration(std::shared_ptr<core::Repository> repo,
                             std::shared_ptr<core::Repository> flow_file_repo,
                             std::shared_ptr<io::StreamFactory> stream_factory,
                             std::shared_ptr<Configure> configuration,
                             const std::string path)
      : CoreComponent(core::getClassName<FlowConfiguration>()),
        flow_file_repo_(flow_file_repo),
        config_path_(path),
        stream_factory_(stream_factory),
        logger_(logging::LoggerFactory<FlowConfiguration>::getLogger()) {
    controller_services_ = std::make_shared<
        core::controller::ControllerServiceMap>();
    service_provider_ = std::make_shared<
        core::controller::StandardControllerServiceProvider>(
        controller_services_, nullptr, configuration);
  }

  virtual ~FlowConfiguration();

  // Create Processor (Node/Input/Output Port) based on the name
  std::shared_ptr<core::Processor> createProcessor(std::string name,
                                                   uuid_t uuid);
  // Create Root Processor Group
  std::unique_ptr<core::ProcessGroup> createRootProcessGroup(std::string name,
                                                             uuid_t uuid);

  std::shared_ptr<core::controller::ControllerServiceNode> createControllerService(
      const std::string &class_name, const std::string &name, uuid_t uuid);

  // Create Remote Processor Group
  std::unique_ptr<core::ProcessGroup> createRemoteProcessGroup(std::string name,
                                                               uuid_t uuid);
  // Create Connection
  std::shared_ptr<minifi::Connection> createConnection(std::string name,
                                                       uuid_t uuid);
  // Create Provenance Report Task
  std::shared_ptr<core::Processor> createProvenanceReportTask(void);

  /**
   * Returns the configuration path string
   * @return config_path_
   */
  const std::string &getConfigurationPath() {
    return config_path_;
  }

  virtual std::unique_ptr<core::ProcessGroup> getRoot() {
    return getRoot(config_path_);
  }

  /**
   * Base implementation that returns a null root pointer.
   * @return Extensions should return a non-null pointer in order to
   * properly configure flow controller.
   */
  virtual std::unique_ptr<core::ProcessGroup> getRoot(
      const std::string &from_config) {
    return nullptr;
  }

  std::shared_ptr<core::controller::StandardControllerServiceProvider> &getControllerServiceProvider() {
    return service_provider_;
  }

 protected:

  // service provider reference.
  std::shared_ptr<core::controller::StandardControllerServiceProvider> service_provider_;
  // based, shared controller service map.
  std::shared_ptr<core::controller::ControllerServiceMap> controller_services_;
  // configuration path
  std::string config_path_;
  // flow file repo
  std::shared_ptr<core::Repository> flow_file_repo_;
  // stream factory
  std::shared_ptr<io::StreamFactory> stream_factory_;
  
 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_FLOWCONFIGURATION_H_ */

