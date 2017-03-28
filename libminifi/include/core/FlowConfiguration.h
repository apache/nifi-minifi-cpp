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

#include "core/core.h"
#include "Connection.h"
#include "RemoteProcessorGroupPort.h"
#include "provenance/Provenance.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/TailFile.h"
#include "processors/ListenSyslog.h"
#include "processors/GenerateFlowFile.h"
#include "processors/RealTimeDataCollector.h"
#include "processors/ListenHTTP.h"
#include "processors/LogAttribute.h"
#include "processors/ExecuteProcess.h"
#include "processors/AppendHostInfo.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessGroup.h"

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
  FlowConfiguration(std::shared_ptr<core::Repository> repo,
                    std::shared_ptr<core::Repository> flow_file_repo,
                    const std::string path)
      : CoreComponent(core::getClassName<FlowConfiguration>()),
        flow_file_repo_(flow_file_repo),
        config_path_(path) {

  }

  virtual ~FlowConfiguration();

  // Create Processor (Node/Input/Output Port) based on the name
  std::shared_ptr<core::Processor> createProcessor(std::string name,
                                                   uuid_t uuid);
  // Create Root Processor Group
  std::unique_ptr<core::ProcessGroup> createRootProcessGroup(std::string name,
                                                             uuid_t uuid);
  // Create Remote Processor Group
  std::unique_ptr<core::ProcessGroup> createRemoteProcessGroup(std::string name,
                                                               uuid_t uuid);
  // Create Connection
  std::shared_ptr<minifi::Connection> createConnection(std::string name,
                                                       uuid_t uuid);

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

 protected:
  // configuration path
  std::string config_path_;
  // flow file repo
  std::shared_ptr<core::Repository> flow_file_repo_;

};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_FLOWCONFIGURATION_H_ */

