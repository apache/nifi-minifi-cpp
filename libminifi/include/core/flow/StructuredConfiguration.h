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
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "core/FlowConfiguration.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessorConfig.h"
#include "Exception.h"
#include "io/StreamFactory.h"
#include "io/validation.h"
#include "sitetosite/SiteToSite.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"
#include "utils/file/FileSystem.h"
#include "core/flow/Node.h"

namespace org::apache::nifi::minifi::core {

static constexpr char const* CONFIG_YAML_FLOW_CONTROLLER_KEY = "Flow Controller";
static constexpr char const* CONFIG_YAML_PROCESSORS_KEY = "Processors";
static constexpr char const* CONFIG_YAML_CONTROLLER_SERVICES_KEY = "Controller Services";
static constexpr char const* CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY = "Remote Processing Groups";
static constexpr char const* CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY_V3 = "Remote Process Groups";
static constexpr char const* CONFIG_YAML_PROVENANCE_REPORT_KEY = "Provenance Reporting";
static constexpr char const* CONFIG_YAML_FUNNELS_KEY = "Funnels";
static constexpr char const* CONFIG_YAML_INPUT_PORTS_KEY = "Input Ports";
static constexpr char const* CONFIG_YAML_OUTPUT_PORTS_KEY = "Output Ports";

#define YAML_CONFIGURATION_USE_REGEX

// Disable regex in EL for incompatible compilers
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
#undef YAML_CONFIGURATION_USE_REGEX
#endif

class StructuredConfiguration : public FlowConfiguration {
 public:
  StructuredConfiguration(ConfigurationContext ctx, std::shared_ptr<logging::Logger> logger);

  /**
   * Iterates all component property validation rules and checks that configured state
   * is valid. If state is determined to be invalid, conf parsing ends and an error is raised.
   *
   * @param component
   * @param component_name
   * @param yaml_section
   */
  void validateComponentProperties(ConfigurableComponent& component, const std::string &component_name, const std::string &yaml_section) const;

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
  std::unique_ptr<core::ProcessGroup> getRootFrom(const flow::Node& root_node);

  std::unique_ptr<core::ProcessGroup> createProcessGroup(const flow::Node& node, bool is_root = false);

  std::unique_ptr<core::ProcessGroup> parseProcessGroup(const flow::Node& header_node, const flow::Node& node, bool is_root = false);
  /**
   * Parses a processor from its corresponding YAML config node and adds
   * it to a parent ProcessGroup. The processorNode argument must point
   * to a YAML::Node containing the processor configuration. A Processor
   * object will be created a added to the parent ProcessGroup specified
   * by the parent argument.
   *
   * @param processorsNode the YAML::Node containing the processor configuration
   * @param parent         the parent ProcessGroup to which the the created
   *                       Processor should be added
   */
  void parseProcessorNode(const flow::Node& processors_node, core::ProcessGroup* parent);

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
  void parsePort(const flow::Node& port_node, core::ProcessGroup* parent, sitetosite::TransferDirection direction);

  /**
   * Parses the root level YAML node for the flow configuration and
   * returns a ProcessGroup containing the tree of flow configuration
   * objects.
   *
   * @param rootFlowNode
   * @return
   */
  std::unique_ptr<core::ProcessGroup> parseRootProcessGroup(const flow::Node& root_flow_node);

  // Process Property YAML
  void parseProcessorProperty(const flow::Node& doc, const flow::Node& node, std::shared_ptr<core::Processor> processor);
  /**
   * Parse controller services
   * @param controllerServicesNode controller services YAML node.
   * @param parent parent process group.
   */
  void parseControllerServices(const flow::Node& controller_services_node);

  /**
   * Parses the Connections section of a configuration YAML.
   * The resulting Connections are added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Connections section
   *                 of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the connections that are parsed
   */
  void parseConnection(const flow::Node& node, core::ProcessGroup* parent);

  /**
   * Parses the Remote Process Group section of a configuration YAML.
   * The resulting Process Group is added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Remote Process Group
   *                 section of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the process groups that are parsed
   */
  void parseRemoteProcessGroup(const flow::Node& node, core::ProcessGroup* parent);

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
  void parseProvenanceReporting(const flow::Node& report_node, core::ProcessGroup* parentGroup);

  /**
   * A helper function to parse the Properties flow::Node YAML for a processor.
   *
   * @param propertiesNode the YAML::Node containing the properties
   * @param processor      the Processor to which to add the resulting properties
   */
  void parsePropertiesNode(const flow::Node& properties_node, core::ConfigurableComponent& component, const std::string& component_name, const std::string& yaml_section);

  /**
   * Parses the Funnels section of a configuration YAML.
   * The resulting Funnels are added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Funnels section
   *                 of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the funnels that are parsed
   */
  void parseFunnels(const flow::Node& node, core::ProcessGroup* parent);

  /**
   * Parses the Input/Output Ports section of a configuration YAML.
   * The resulting ports are added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Input/Output Ports section
   *                 of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the funnels that are parsed
   */
  void parsePorts(const flow::Node& node, core::ProcessGroup* parent, PortType port_type);

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
  std::string getOrGenerateId(const flow::Node& node, const std::string& id_field = "id");

  std::string getRequiredIdField(const flow::Node& node, std::string_view yaml_section = "", std::string_view error_message = "");

  /**
   * This is a helper function for getting an optional value, if it exists.
   * If it does not exist, returns the provided default value.
   *
   * @param yamlNode     the YAML node to check
   * @param fieldName    the optional field key
   * @param defaultValue the default value to use if field is not set
   * @param yamlSection  [optional] the top level section of the YAML config
   *                       for the yamlNode. This is used fpr generating a
   *                       useful info message for troubleshooting.
   * @param infoMessage  [optional] the info message string to use if
   *                       the optional field is missing. If not provided,
   *                       a default info message will be generated.
   */
  std::string getOptionalField(const flow::Node& node, const std::string& field_name, const std::string& default_value, const std::string& section = "", const std::string& info_message = "");

  static std::shared_ptr<utils::IdGenerator> id_generator_;
  std::unordered_set<std::string> uuids_;
  std::shared_ptr<logging::Logger> logger_;

 private:
  PropertyValue getValidatedProcessorPropertyForDefaultTypeInfo(const core::Property& property_from_processor, const flow::Node& property_value_node);
  void parsePropertyValueSequence(const std::string& property_name, const flow::Node& property_value_node, core::ConfigurableComponent& component);
  void parseSingleProperty(const std::string& property_name, const flow::Node& property_value_node, core::ConfigurableComponent& processor);
  void parsePropertyNodeElement(const std::string& propertyName, const flow::Node& property_value_node, core::ConfigurableComponent& processor);
  void addNewId(const std::string& uuid);

  /**
   * Raises a human-readable configuration error for the given configuration component/section.
   *
   * @param component_name
   * @param yaml_section
   * @param reason
   */
  void raiseComponentError(const std::string &component_name, const std::string &yaml_section, const std::string &reason) const;
};

}  // namespace org::apache::nifi::minifi::core

