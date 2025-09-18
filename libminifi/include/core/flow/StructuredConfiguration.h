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
#include <vector>

#include "core/FlowConfiguration.h"
#include "core/logging/LoggerConfiguration.h"
#include "minifi-cpp/core/ProcessorConfig.h"
#include "minifi-cpp/Exception.h"
#include "io/validation.h"
#include "sitetosite/SiteToSite.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"
#include "utils/file/FileSystem.h"
#include "core/flow/Node.h"
#include "FlowSchema.h"
#include "FlowSerializer.h"

namespace org::apache::nifi::minifi::core::flow {

class StructuredConfiguration : public FlowConfiguration {
 public:
  StructuredConfiguration(ConfigurationContext ctx, std::shared_ptr<logging::Logger> logger);

  /**
   * Iterates all component property validation rules and checks that configured state
   * is valid. If state is determined to be invalid, conf parsing ends and an error is raised.
   *
   * @param component
   * @param component_name
   * @param section
   */
  void validateComponentProperties(ConfigurableComponent& component, const std::string &component_name, const std::string &section) const;

  std::unique_ptr<core::ProcessGroup> getRoot() override;

  std::string serialize(const core::ProcessGroup& process_group) override;

 protected:
  /**
   * Returns a shared pointer to a ProcessGroup object containing the
   * flow configuration.
   *
   * @param root_node a pointer to a Node object containing the root
   *                       node of the parsed document
   * @return             the root ProcessGroup node of the flow
   *                       configuration tree
   */
  std::unique_ptr<core::ProcessGroup> getRootFrom(const Node& root_node, FlowSchema schema);

  std::unique_ptr<core::ProcessGroup> createProcessGroup(const Node& node, bool is_root = false);

  std::unique_ptr<core::ProcessGroup> parseProcessGroup(const Node& header_node, const Node& node, bool is_root = false);

  /**
   * Parses processors from its corresponding config node and adds
   * them to a parent ProcessGroup. The processors_node argument must point
   * to a Node containing the processors configuration. Processor
   * objects will be created and added to the parent ProcessGroup specified
   * by the parent argument.
   *
   * @param processors_node  the Node containing the processor configuration
   * @param parent              the parent ProcessGroup to which the the created
   *                            Processor should be added
   */
  void parseProcessorNode(const Node& processors_node, core::ProcessGroup* parent);

  /**
   * Parses a port from its corresponding config node and adds
   * it to a parent ProcessGroup. The port_node argument must point
   * to a Node containing the port configuration. A RemoteProcessorGroupPort
   * object will be created a added to the parent ProcessGroup specified
   * by the parent argument.
   *
   * @param port_node  the Node containing the port configuration
   * @param parent    the parent ProcessGroup for the port
   * @param direction the TransferDirection of the port
   */
  void parseRPGPort(const Node& port_node, core::ProcessGroup* parent, sitetosite::TransferDirection direction);

  /**
   * Parses the root level node for the flow configuration and
   * returns a ProcessGroup containing the tree of flow configuration
   * objects.
   *
   * @param root_flow_node
   * @return
   */
  std::unique_ptr<core::ProcessGroup> parseRootProcessGroup(const Node& root_flow_node);

  void parseParameterContexts(const Node& parameter_contexts_node, const Node& parameter_providers_node);
  void parseControllerServices(const Node& controller_services_node, core::ProcessGroup* parent_group);

  /**
   * Parses the Connections section of a configuration.
   * The resulting Connections are added to the parent ProcessGroup.
   *
   * @param connection_node_seq   the Node containing the Connections section
   *                              of the configuration
   * @param parent                the root node of flow configuration to which
   *                              to add the connections that are parsed
   */
  void parseConnection(const Node& connection_node_seq, core::ProcessGroup* parent);

  /**
   * Parses the Remote Process Group section of a configuration.
   * The resulting Process Group is added to the parent ProcessGroup.
   *
   * @param rpg_node_seq  the Node containing the Remote Process Group
   *                      section of the configuration
   * @param parent        the root node of flow configuration to which
   *                      to add the process groups that are parsed
   */
  void parseRemoteProcessGroup(const Node& rpg_node_seq, core::ProcessGroup* parent);

  /**
   * Parses the Provenance Reporting section of a configuration.
   * The resulting Provenance Reporting processor is added to the
   * parent ProcessGroup.
   *
   * @param report_node  the Node containing the provenance
   *                      reporting configuration
   * @param parent_group the root node of flow configuration to which
   *                      to add the provenance reporting config
   */
  void parseProvenanceReporting(const Node& report_node, core::ProcessGroup* parent_group);

  /**
   * A helper function to parse the Properties Node for a processor.
   *
   * @param properties_node the Node containing the properties
   * @param processor      the Processor to which to add the resulting properties
   */
  void parsePropertiesNode(const Node& properties_node, core::ConfigurableComponent& component, const std::string& component_name, ParameterContext* parameter_context);

  /**
   * Parses the Funnels section of a configuration.
   * The resulting Funnels are added to the parent ProcessGroup.
   *
   * @param node   the Node containing the Funnels section
   *                 of the configuration
   * @param parent the root node of flow configuration to which
   *                 to add the funnels that are parsed
   */
  void parseFunnels(const Node& node, core::ProcessGroup* parent);

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

  void parseParameterContext(const flow::Node& node, core::ProcessGroup& parent);

  /**
   * A helper function for parsing or generating optional id fields.
   *
   * In parsing flow configurations for config schema v1, the
   * 'id' field of most component types that contains a UUID is optional.
   * This function will check for the existence of the specified
   * idField in the specified node. If present, the field will be parsed
   * as a UUID and the UUID string will be returned. If not present, a
   * random UUID string will be generated and returned.
   *
   * @param node     a pointer to the Node that will be checked for the
   *                   presence of an idField
   * @param id_field  the string of the name of the idField to check for. This
   *                   is optional and defaults to 'id'
   * @return         the parsed or generated UUID string
   */
  std::string getOrGenerateId(const Node& node);

  std::string getRequiredIdField(const Node& node, std::string_view error_message = "");

  /**
   * This is a helper function for getting an optional value, if it exists.
   * If it does not exist, returns the provided default value.
   *
   * @param node         the flow node to check
   * @param field_name    the optional field key
   * @param default_value the default value to use if field is not set
   * @param section  [optional] the top level section of the config
   *                       for the node. This is used fpr generating a
   *                       useful info message for troubleshooting.
   * @param info_message  [optional] the info message string to use if
   *                       the optional field is missing. If not provided,
   *                       a default info message will be generated.
   */
  std::string getOptionalField(const Node& node, const std::vector<std::string>& field_name, const std::string& default_value, const std::string& info_message = "");

  FlowSchema schema_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
  std::unordered_set<std::string> uuids_;
  std::unique_ptr<FlowSerializer> flow_serializer_;
  std::shared_ptr<logging::Logger> logger_;

 private:
  std::optional<std::string> getReplacedParametersValueOrDefault(std::string_view property_name,
    bool is_sensitive,
    std::optional<std::string_view> default_value,
    const Node& property_value_node,
    ParameterContext* parameter_context);
  void parsePropertyValueSequence(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& component, ParameterContext* parameter_context);
  void parseSingleProperty(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& component, ParameterContext* parameter_context);
  void parsePropertyNodeElement(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& component, ParameterContext* parameter_context);
  void addNewId(const std::string& uuid);

  /**
   * Raises a human-readable configuration error for the given configuration component/section.
   *
   * @param component_name
   * @param section
   * @param reason
   */
  void raiseComponentError(const std::string &component_name, const std::string &section, const std::string &reason) const;
  void parseParameterProvidersNode(const Node& parameter_providers_node);
  void parseParameterContextsNode(const Node& parameter_contexts_node);
  void parseParameterContextInheritance(const Node& parameter_contexts_node);
  void verifyNoInheritanceCycles() const;
};

}  // namespace org::apache::nifi::minifi::core::flow

