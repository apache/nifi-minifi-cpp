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

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/Core.h"
#include "Connection.h"
#include "RemoteProcessorGroupPort.h"
#include "core/controller/ControllerServiceNode.h"
#include "core/controller/StandardControllerServiceProvider.h"
#include "provenance/Provenance.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"

#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessGroup.h"
#include "io/StreamFactory.h"
#include "core/state/nodes/FlowInformation.h"
#include "utils/file/FileSystem.h"
#include "utils/ChecksumCalculator.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class static_initializers {
 public:
  std::vector<std::string> statics_sl_funcs_;
  std::mutex atomic_initialization_;
};

extern static_initializers &get_static_functions();

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
  explicit FlowConfiguration(std::shared_ptr<core::Repository> repo, std::shared_ptr<core::Repository> flow_file_repo,
                             std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<io::StreamFactory> stream_factory,
                             std::shared_ptr<Configure> configuration, const std::optional<std::string>& path,
                             std::shared_ptr<utils::file::FileSystem> filesystem = std::make_shared<utils::file::FileSystem>());

  ~FlowConfiguration() override;

  // Create Processor (Node/Input/Output Port) based on the name
  std::shared_ptr<core::Processor> createProcessor(const std::string &name, const utils::Identifier &uuid);
  std::shared_ptr<core::Processor> createProcessor(const std::string &name, const std::string &fullname, const utils::Identifier &uuid);
  // Create Root Processor Group

  std::unique_ptr<core::ProcessGroup> createRootProcessGroup(const std::string &name, const utils::Identifier &uuid, int version);
  std::unique_ptr<core::ProcessGroup> createSimpleProcessGroup(const std::string &name, const utils::Identifier &uuid, int version);
  std::unique_ptr<core::ProcessGroup> createRemoteProcessGroup(const std::string &name, const utils::Identifier &uuid);

  std::shared_ptr<core::controller::ControllerServiceNode> createControllerService(const std::string &class_name, const std::string &full_class_name, const std::string &name,
      const utils::Identifier &uuid);

  // Create Connection
  std::shared_ptr<minifi::Connection> createConnection(const std::string &name, const utils::Identifier &uuid) const;
  // Create Provenance Report Task
  std::shared_ptr<core::Processor> createProvenanceReportTask(void);

  std::shared_ptr<state::response::FlowVersion> getFlowVersion() const {
    return flow_version_;
  }

  std::shared_ptr<Configure> getConfiguration() {  // cannot be const as getters mutate the underlying map
    return configuration_;
  }

  bool persist(const std::string& configuration);

  /**
   * Returns the configuration path string
   * @return config_path_
   */
  const std::optional<std::string> &getConfigurationPath() {
    return config_path_;
  }

  virtual std::unique_ptr<core::ProcessGroup> getRoot() {
    return nullptr;
  }

  std::unique_ptr<core::ProcessGroup> updateFromPayload(const std::string& url, const std::string& yamlConfigPayload);

  virtual std::unique_ptr<core::ProcessGroup> getRootFromPayload(const std::string& /*yamlConfigPayload*/) {
    return nullptr;
  }

  std::shared_ptr<core::controller::StandardControllerServiceProvider> &getControllerServiceProvider() {
    return service_provider_;
  }

  utils::ChecksumCalculator& getChecksumCalculator() { return checksum_calculator_; }

 protected:
  // service provider reference.
  std::shared_ptr<core::controller::StandardControllerServiceProvider> service_provider_;
  // based, shared controller service map.
  std::shared_ptr<core::controller::ControllerServiceMap> controller_services_;
  // configuration path
  std::optional<std::string> config_path_;
  // flow file repo
  std::shared_ptr<core::Repository> flow_file_repo_;
  // content repository.
  std::shared_ptr<core::ContentRepository> content_repo_;
  // stream factory
  std::shared_ptr<io::StreamFactory> stream_factory_;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<state::response::FlowVersion> flow_version_;
  std::shared_ptr<utils::file::FileSystem> filesystem_;
  utils::ChecksumCalculator checksum_calculator_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_FLOWCONFIGURATION_H_

