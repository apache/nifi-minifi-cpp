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
#ifndef LIBMINIFI_INCLUDE_CORE_YAMLCONFIGURATION_H_
#define LIBMINIFI_INCLUDE_CORE_YAMLCONFIGURATION_H_

#include "core/ProcessorConfig.h"
#include "yaml-cpp/yaml.h"
#include "../FlowConfiguration.h"
#include "Site2SiteClientProtocol.h"
#include <string>
#include "io/validation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

#define DEFAULT_FLOW_YAML_FILE_NAME "conf/flow.yml"
#define CONFIG_YAML_PROCESSORS_KEY "Processors"

class YamlConfiguration : public FlowConfiguration {

 public:
  YamlConfiguration(std::shared_ptr<core::Repository> repo,
                    std::shared_ptr<core::Repository> flow_file_repo,
                    const std::string path = DEFAULT_FLOW_YAML_FILE_NAME)
      : FlowConfiguration(repo, flow_file_repo, path) {
    if (IsNullOrEmpty(config_path_)) {
      config_path_ = DEFAULT_FLOW_YAML_FILE_NAME;
    }
  }

  virtual ~YamlConfiguration() {

  }

  std::unique_ptr<core::ProcessGroup> getRoot(const std::string &from_config) {

    YAML::Node flow = YAML::LoadFile(from_config);

    YAML::Node flowControllerNode = flow["Flow Controller"];
    YAML::Node processorsNode = flow[CONFIG_YAML_PROCESSORS_KEY];
    YAML::Node connectionsNode = flow["Connections"];
    YAML::Node remoteProcessingGroupNode = flow["Remote Processing Groups"];
    YAML::Node provenanceReportNode = flow["Provenance Reporting"];

    // Create the root process group
    core::ProcessGroup * root = parseRootProcessGroupYaml(flowControllerNode);
    parseProcessorNodeYaml(processorsNode, root);
    parseRemoteProcessGroupYaml(&remoteProcessingGroupNode, root);
    parseConnectionYaml(&connectionsNode, root);
    parseProvenanceReportingYaml(&provenanceReportNode, root);

    return std::unique_ptr<core::ProcessGroup>(root);

  }
 protected:
  // Process Processor Node YAML
  void parseProcessorNodeYaml(YAML::Node processorNode,
                              core::ProcessGroup * parent);
  // Process Port YAML
  void parsePortYaml(YAML::Node *portNode, core::ProcessGroup *parent,
                     TransferDirection direction);
  // Process Root Processor Group YAML
  core::ProcessGroup *parseRootProcessGroupYaml(YAML::Node rootNode);
  // Process Property YAML
  void parseProcessorPropertyYaml(YAML::Node *doc, YAML::Node *node,
                                  std::shared_ptr<core::Processor> processor);
  // Process connection YAML
  void parseConnectionYaml(YAML::Node *node, core::ProcessGroup * parent);
  // Process Remote Process Group YAML
  void parseRemoteProcessGroupYaml(YAML::Node *node,
                                   core::ProcessGroup * parent);
  // Process Provenance Report YAML
  void parseProvenanceReportingYaml(YAML::Node *reportNode, core::ProcessGroup * parentGroup);
  // Parse Properties Node YAML for a processor
  void parsePropertiesNodeYaml(YAML::Node *propertiesNode,
                               std::shared_ptr<core::Processor> processor);
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_YAMLCONFIGURATION_H_ */
