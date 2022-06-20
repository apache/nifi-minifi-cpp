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
#ifndef LIBMINIFI_INCLUDE_CORE_YAML_YAMLCONFIGURATION_H_
#define LIBMINIFI_INCLUDE_CORE_YAML_YAMLCONFIGURATION_H_

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
#include "yaml-cpp/yaml.h"

class YamlConfigurationTestAccessor;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

static constexpr char const* CONFIG_YAML_FLOW_CONTROLLER_KEY = "Flow Controller";
static constexpr char const* CONFIG_YAML_PROCESSORS_KEY = "Processors";
static constexpr char const* CONFIG_YAML_CONTROLLER_SERVICES_KEY = "Controller Services";
static constexpr char const* CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY = "Remote Processing Groups";
static constexpr char const* CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY_V3 = "Remote Process Groups";
static constexpr char const* CONFIG_YAML_PROVENANCE_REPORT_KEY = "Provenance Reporting";
static constexpr char const* CONFIG_YAML_FUNNELS_KEY = "Funnels";

#define YAML_CONFIGURATION_USE_REGEX

// Disable regex in EL for incompatible compilers
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
#undef YAML_CONFIGURATION_USE_REGEX
#endif

class YamlConfiguration : public FlowConfiguration {
 public:
  explicit YamlConfiguration(const std::shared_ptr<core::Repository>& repo, const std::shared_ptr<core::Repository>& flow_file_repo,
                             const std::shared_ptr<core::ContentRepository>& content_repo, const std::shared_ptr<io::StreamFactory>& stream_factory,
                             const std::shared_ptr<Configure>& configuration, const std::optional<std::string>& path = {},
                             const std::shared_ptr<utils::file::FileSystem>& filesystem = std::make_shared<utils::file::FileSystem>());

  ~YamlConfiguration() override = default;

  /**
   * Returns a shared pointer to a ProcessGroup object containing the
   * flow configuration.
   *
   * @return               the root ProcessGroup node of the flow
   *                        configuration tree
   */
  std::unique_ptr<core::ProcessGroup> getRoot() override {
    if (!config_path_) {
      logger_->log_error("Cannot instantiate flow, no config file is set.");
      throw Exception(ExceptionType::FLOW_EXCEPTION, "No config file specified");
    }
    const auto configuration = filesystem_->read(config_path_.value());
    if (!configuration) {
      return nullptr;
    }
    try {
      YAML::Node rootYamlNode = YAML::Load(configuration.value());
      return getYamlRoot(rootYamlNode);
    } catch(...) {
      logger_->log_error("Invalid yaml configuration file");
      throw;
    }
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
  std::unique_ptr<core::ProcessGroup> getYamlRoot(std::istream &yamlConfigStream) {
    try {
      YAML::Node rootYamlNode = YAML::Load(yamlConfigStream);
      return getYamlRoot(rootYamlNode);
    } catch (const YAML::ParserException &pe) {
      logger_->log_error(pe.what());
      std::rethrow_exception(std::current_exception());
    }
    return nullptr;
  }

  /**
   * Returns a shared pointer to a ProcessGroup object containing the
   * flow configuration. The yamlConfigPayload argument must be
   * a payload for the raw YAML configuration.
   *
   * @param yamlConfigPayload an input payload for the raw YAML configuration
   *                           to be parsed and loaded into the flow
   *                           configuration tree
   * @return                 the root ProcessGroup node of the flow
   *                           configuration tree
   */
  std::unique_ptr<core::ProcessGroup> getRootFromPayload(const std::string &yamlConfigPayload) override {
    try {
      YAML::Node rootYamlNode = YAML::Load(yamlConfigPayload);
      return getYamlRoot(rootYamlNode);
    } catch (const YAML::ParserException &pe) {
      logger_->log_error(pe.what());
      std::rethrow_exception(std::current_exception());
    }
    return nullptr;
  }

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
  std::unique_ptr<core::ProcessGroup> getYamlRoot(const YAML::Node& rootYamlNode);

  std::unique_ptr<core::ProcessGroup> createProcessGroup(const YAML::Node& yamlNode, bool is_root = false);

  std::unique_ptr<core::ProcessGroup> parseProcessGroupYaml(const YAML::Node& headerNode, const YAML::Node& yamlNode, bool is_root = false);
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
  void parseProcessorNodeYaml(const YAML::Node& processorsNode, core::ProcessGroup* parent);

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
  void parsePortYaml(const YAML::Node& portNode, core::ProcessGroup* parent, sitetosite::TransferDirection direction);

  /**
   * Parses the root level YAML node for the flow configuration and
   * returns a ProcessGroup containing the tree of flow configuration
   * objects.
   *
   * @param rootFlowNode
   * @return
   */
  std::unique_ptr<core::ProcessGroup> parseRootProcessGroupYaml(const YAML::Node& rootFlowNode);

  // Process Property YAML
  void parseProcessorPropertyYaml(const YAML::Node& doc, const YAML::Node& node, std::shared_ptr<core::Processor> processor);
  /**
   * Parse controller services
   * @param controllerServicesNode controller services YAML node.
   * @param parent parent process group.
   */
  void parseControllerServices(const YAML::Node& controllerServicesNode);

  /**
   * Parses the Connections section of a configuration YAML.
   * The resulting Connections are added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Connections section
   *                 of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the connections that are parsed
   */
  void parseConnectionYaml(const YAML::Node& node, core::ProcessGroup* parent);

  /**
   * Parses the Remote Process Group section of a configuration YAML.
   * The resulting Process Group is added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Remote Process Group
   *                 section of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the process groups that are parsed
   */
  void parseRemoteProcessGroupYaml(const YAML::Node& node, core::ProcessGroup* parent);

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
  void parseProvenanceReportingYaml(const YAML::Node& reportNode, core::ProcessGroup* parentGroup);

  /**
   * A helper function to parse the Properties Node YAML for a processor.
   *
   * @param propertiesNode the YAML::Node containing the properties
   * @param processor      the Processor to which to add the resulting properties
   */
  void parsePropertiesNodeYaml(const YAML::Node& propertiesNode, core::ConfigurableComponent& component, const std::string& component_name, const std::string& yaml_section);

  /**
   * Parses the Funnels section of a configuration YAML.
   * The resulting Funnels are added to the parent ProcessGroup.
   *
   * @param node   the YAML::Node containing the Funnels section
   *                 of the configuration YAML
   * @param parent the root node of flow configuration to which
   *                 to add the funnels that are parsed
   */
  void parseFunnelsYaml(const YAML::Node& node, core::ProcessGroup* parent);

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
  std::string getOrGenerateId(const YAML::Node& yamlNode, const std::string& idField = "id");
  std::string getRequiredIdField(const YAML::Node& yaml_node, std::string_view yaml_section = "", std::string_view error_message = "");

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
  YAML::Node getOptionalField(const YAML::Node& yamlNode, const std::string& fieldName, const YAML::Node& defaultValue, const std::string& yamlSection = "", const std::string& infoMessage = "");

 protected:
  std::shared_ptr<io::StreamFactory> stream_factory_;

 private:
  PropertyValue getValidatedProcessorPropertyForDefaultTypeInfo(const core::Property& propertyFromProcessor, const YAML::Node& propertyValueNode);
  void parsePropertyValueSequence(const std::string& propertyName, const YAML::Node& propertyValueNode, core::ConfigurableComponent& component);
  void parseSingleProperty(const std::string& propertyName, const YAML::Node& propertyValueNode, core::ConfigurableComponent& processor);
  void parsePropertyNodeElement(const std::string& propertyName, const YAML::Node& propertyValueNode, core::ConfigurableComponent& processor);
  void addNewId(const std::string& uuid);

  std::shared_ptr<logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
  std::unordered_set<std::string> uuids_;

  /**
   * Raises a human-readable configuration error for the given configuration component/section.
   *
   * @param component_name
   * @param yaml_section
   * @param reason
   */
  void raiseComponentError(const std::string &component_name, const std::string &yaml_section, const std::string &reason) const;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_YAML_YAMLCONFIGURATION_H_
