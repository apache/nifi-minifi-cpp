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
#include "processors/LoadProcessors.h"
#include "../FlowConfiguration.h"
#include "Site2SiteClientProtocol.h"
#include <string>
#include "io/validation.h"
#include "io/StreamFactory.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

#define DEFAULT_FLOW_YAML_FILE_NAME "conf/flow.yml"
#define CONFIG_YAML_FLOW_CONTROLLER_KEY "Flow Controller"
#define CONFIG_YAML_PROCESSORS_KEY "Processors"
#define CONFIG_YAML_CONNECTIONS_KEY "Connections"
#define CONFIG_YAML_CONTROLLER_SERVICES_KEY "Controller Services"
#define CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY "Remote Processing Groups"
#define CONFIG_YAML_PROVENANCE_REPORT_KEY "Provenance Reporting"

class YamlConfiguration : public FlowConfiguration {

 public:
  explicit YamlConfiguration(std::shared_ptr<core::Repository> repo,
                    std::shared_ptr<core::Repository> flow_file_repo,
                    std::shared_ptr<io::StreamFactory> stream_factory,
                    std::shared_ptr<Configure> configuration,
                    const std::string path = DEFAULT_FLOW_YAML_FILE_NAME)
      : FlowConfiguration(repo, flow_file_repo, stream_factory, configuration,
                          path),
         logger_(logging::LoggerFactory<YamlConfiguration>::getLogger()) {
    stream_factory_ = stream_factory;
    if (IsNullOrEmpty(config_path_)) {
      config_path_ = DEFAULT_FLOW_YAML_FILE_NAME;
    }
  }

  virtual ~YamlConfiguration() {

  }

  /**
   * Returns a shared pointer to a ProcessGroup object containing the
   * flow configuration. The yamlConfigFile argument is the location
   * of a YAML file containing the flow configuration.
   *
   * @param yamlConfigFile a string holding the location of the YAML file
   *                        to be loaded into a flow configuration tree
   * @return               the root ProcessGroup node of the flow
   *                        configuration tree
   */
  std::unique_ptr<core::ProcessGroup> getRoot(const std::string &yamlConfigFile) {
    YAML::Node rootYamlNode = YAML::LoadFile(yamlConfigFile);
    return getRoot(&rootYamlNode);
  }

  /**
   * Returns a shared pointer to a ProcessGroup object containing the
   * flow configuration. The yamlConfigStream argument must point to
   * an input stream for the raw YAML configuration.
   *
   * @param yamlConfigStream an input stream for the raw YAML configutation
   *                           to be parsed and loaded into the flow
   *                           configuration tree
   * @return                 the root ProcessGroup node of the flow
   *                           configuration tree
   */
  std::unique_ptr<core::ProcessGroup> getRoot(std::istream &yamlConfigStream) {
    YAML::Node rootYamlNode = YAML::Load(yamlConfigStream);
    return getRoot(&rootYamlNode);
  }

 protected:

  /**
   * Returns a shared pointer to a ProcessGroup object containing the
   * flow configuration. The rootYamlNode argument must point to
   * an YAML::Node object containing the root node of the parsed YAML
   * for the flow configuration.
   *
   * @param rootYamlNode a pointer to a YAML::Node object containing the root
   *                       node of the parsed YAML document
   * @return             the root ProcessGroup node of the flow
   *                       configuration tree
   */
  std::unique_ptr<core::ProcessGroup> getRoot(YAML::Node *rootYamlNode) {
    YAML::Node rootYaml = *rootYamlNode;
    YAML::Node flowControllerNode = rootYaml[CONFIG_YAML_FLOW_CONTROLLER_KEY];
    YAML::Node processorsNode = rootYaml[CONFIG_YAML_PROCESSORS_KEY];
    YAML::Node connectionsNode = rootYaml[CONFIG_YAML_CONNECTIONS_KEY];
    YAML::Node controllerServiceNode =
        rootYaml[CONFIG_YAML_CONTROLLER_SERVICES_KEY];
    YAML::Node remoteProcessingGroupsNode =
        rootYaml[CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY];
    YAML::Node provenanceReportNode =
        rootYaml[CONFIG_YAML_PROVENANCE_REPORT_KEY];

    parseControllerServices(&controllerServiceNode);
    // Create the root process group
    core::ProcessGroup * root = parseRootProcessGroupYaml(flowControllerNode);
    parseProcessorNodeYaml(processorsNode, root);
    parseRemoteProcessGroupYaml(&remoteProcessingGroupsNode, root);
    parseConnectionYaml(&connectionsNode, root);
    parseProvenanceReportingYaml(&provenanceReportNode, root);

    // set the controller services into the root group.
    for (auto controller_service : controller_services_
        ->getAllControllerServices()) {
      root->addControllerService(controller_service->getName(),
                                 controller_service);
      root->addControllerService(controller_service->getUUIDStr(),
                                 controller_service);
    }

    return std::unique_ptr<core::ProcessGroup>(root);
  }

  /**
   * Parses a processor from its corresponding YAML config node and adds
   * it to a parent ProcessGroup. The processorNode argument must point
   * to a YAML::Node containing the processor configuration. A Processor
   * object will be created a added to the parent ProcessGroup specified
   * by the parent argument.
   *
   * @param processorNode the YAML::Node containing the processor configuration
   * @param parent        the parent ProcessGroup to which the the created
   *                        Processor should be added
   */
  void parseProcessorNodeYaml(YAML::Node processorNode,
                              core::ProcessGroup * parent);

  /**
   * Parses a port from its corressponding YAML config node and adds
   * it to a parent ProcessGroup. The portNode argument must point
   * to a YAML::Node containing the port configuration. A RemoteProcessorGroupPort
   * object will be created a added to the parent ProcessGroup specified
   * by the parent argument.
   *
   * @param portNode  the YAML::Node containing the port configuration
   * @param parent    the parent ProcessGroup for the port
   * @param direction the TransferDirection of the port
   */
  void parsePortYaml(YAML::Node *portNode, core::ProcessGroup *parent,
                     TransferDirection direction);

  /**
   * Parses the root level YAML node for the flow configuration and
   * returns a ProcessGroup containing the tree of flow configuration
   * objects.
   *
   * @param rootNode
   * @return
   */
  core::ProcessGroup *parseRootProcessGroupYaml(YAML::Node rootNode);

  // Process Property YAML
  void parseProcessorPropertyYaml(YAML::Node *doc, YAML::Node *node,
                                  std::shared_ptr<core::Processor> processor);
  /**
   * Parse controller services
   * @param controllerServicesNode controller services YAML node.
   * @param parent parent process group.
   */
  void parseControllerServices(YAML::Node *controllerServicesNode);
  // Process connection YAML

  /**
   * Parses the Connections section of a configuration YAML.
   * The resulting Connections are added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Connections section
   *                 of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the connections that are parsed
   */
  void parseConnectionYaml(YAML::Node *node, core::ProcessGroup * parent);

  /**
   * Parses the Remote Process Group section of a configuration YAML.
   * The resulting Process Group is added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Remote Process Group
   *                 section of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the process groups that are parsed
   */
  void parseRemoteProcessGroupYaml(YAML::Node *node,
                                   core::ProcessGroup * parent);

  /**
   * Parses the Provenance Reporting section of a configuration YAML.
   * The resulting Provenance Reporting processor is added to the
   * parent ProcessGroup.
   *
   * @param reportNode  the YAML::Node containing the provenance
   *                      reporting configuration
   * @param parentGroup the root node of flow configuration to which
   *                      to add the provenance reporting config
   */
  void parseProvenanceReportingYaml(YAML::Node *reportNode,
                                    core::ProcessGroup * parentGroup);

  /**
   * A helper function to parse the Properties Node YAML for a processor.
   *
   * @param propertiesNode the YAML::Node containing the properties
   * @param processor      the Processor to which to add the resulting properties
   */
  void parsePropertiesNodeYaml(
      YAML::Node *propertiesNode,
      std::shared_ptr<core::ConfigurableComponent> processor);

  /**
   * A helper function for parsing or generating optional id fields.
   *
   * In parsing YAML flow configurations for config schema v1, the
   * 'id' field of most component types that contains a UUID is optional.
   * This function will check for the existence of the specified
   * idField in the specified yamlNode. If present, the field will be parsed
   * as a UUID and the UUID string will be returned. If not present, a
   * random UUID string will be generated and returned.
   *
   * @param yamlNode a pointer to the YAML::Node that will be checked for the
   *                   presence of an idField
   * @param idField  the string of the name of the idField to check for. This
   *                   is optional and defaults to 'id'
   * @return         the parsed or generated UUID string
   */
  std::string getOrGenerateId(YAML::Node *yamlNode, const std::string &idField =
                                  "id");

  /**
   * This is a helper function for verifying the existence of a required
   * field in a YAML::Node object. If the field is not present, an error
   * message will be logged and a std::invalid_argument exception will be
   * thrown indicating the absence of the required field in the YAML node.
   *
   * @param yamlNode     the YAML node to check
   * @param fieldName    the required field key
   * @param yamlSection  [optional] the top level section of the YAML config
   *                       for the yamlNode. This is used fpr generating a
   *                       useful error message for troubleshooting.
   * @param errorMessage [optional] the error message string to use if
   *                       the required field is missing. If not provided,
   *                       a default error message will be generated.
   *
   * @throws std::invalid_argument if the required field 'fieldName' is
   *                               not present in 'yamlNode'
   */
  void checkRequiredField(YAML::Node *yamlNode, const std::string &fieldName,
                          const std::string &yamlSection = "",
                          const std::string &errorMessage = "");

 protected:
  std::shared_ptr<io::StreamFactory> stream_factory_;
 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_YAMLCONFIGURATION_H_ */
