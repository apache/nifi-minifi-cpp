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
#include "FlowConfigEncryptor.h"

#include "core/extension/ExtensionManager.h"
#include "core/FlowConfiguration.h"
#include "core/flow/AdaptiveConfiguration.h"
#include "core/ProcessGroup.h"
#include "core/RepositoryFactory.h"
#include "core/repository/VolatileContentRepository.h"
#include "Defaults.h"
#include "Utils.h"
#include "utils/file/FileSystem.h"
#include "utils/Id.h"

namespace minifi = org::apache::nifi::minifi;

namespace {
enum class Type {
  Processor, ControllerService
};

struct SensitiveProperty {
  Type type;
  minifi::utils::Identifier component_id;
  std::string component_name;
  std::string property_name;
  std::string property_display_name;
};
}  // namespace

namespace magic_enum::customize {
template<>
constexpr customize_t enum_name<Type>(Type type) noexcept {
  switch (type) {
    case Type::Processor: return "Processor";
    case Type::ControllerService: return "Controller service";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace {
std::vector<SensitiveProperty> listSensitiveProperties(const minifi::core::ProcessGroup &process_group) {
  std::vector<SensitiveProperty> sensitive_properties;

  std::vector<minifi::core::Processor *> processors;
  process_group.getAllProcessors(processors);
  for (const auto *processor : processors) {
    gsl_Expects(processor);
    for (const auto& [_, property] : processor->getProperties()) {
      if (property.isSensitive()) {
        sensitive_properties.push_back(SensitiveProperty{
            .type = Type::Processor,
            .component_id = processor->getUUID(),
            .component_name = processor->getName(),
            .property_name = property.getName(),
            .property_display_name = property.getDisplayName()});
      }
    }
  }

  for (const auto &controller_service_node : process_group.getAllControllerServices()) {
    gsl_Expects(controller_service_node);
    const auto controller_service = controller_service_node->getControllerServiceImplementation();
    gsl_Expects(controller_service);
    for (const auto& [_, property] : controller_service->getProperties()) {
      if (property.isSensitive()) {
        sensitive_properties.push_back(SensitiveProperty{
            .type = Type::ControllerService,
            .component_id = controller_service->getUUID(),
            .component_name = controller_service->getName(),
            .property_name = property.getName(),
            .property_display_name = property.getDisplayName()});
      }
    }
  }

  return sensitive_properties;
}

template<typename Func>
void encryptSensitiveValuesInFlowConfigImpl(
    const minifi::encrypt_config::EncryptionKeys& keys, const std::filesystem::path& minifi_home, const std::filesystem::path& flow_config_path, Func create_overrides) {
  const auto configure = std::make_shared<minifi::Configure>();
  configure->setHome(minifi_home);
  configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  bool encrypt_whole_flow_config_file = (configure->get(minifi::Configure::nifi_flow_configuration_encrypt) | minifi::utils::andThen(minifi::utils::string::toBool)).value_or(false);
  auto encryptor = encrypt_whole_flow_config_file ? minifi::utils::crypto::EncryptionProvider::create(minifi_home) : std::nullopt;
  auto filesystem = std::make_shared<minifi::utils::file::FileSystem>(encrypt_whole_flow_config_file, encryptor);

  minifi::core::extension::ExtensionManager::get().initialize(configure);

  minifi::core::flow::AdaptiveConfiguration adaptive_configuration{minifi::core::ConfigurationContext{
      .flow_file_repo = minifi::core::createRepository("flowfilerepository"),
      .content_repo = std::make_shared<minifi::core::repository::VolatileContentRepository>(),
      .configuration = configure,
      .path = flow_config_path,
      .filesystem = filesystem,
      .sensitive_properties_encryptor = minifi::utils::crypto::EncryptionProvider{minifi::utils::crypto::XSalsa20Cipher{keys.encryption_key}}
  }};

  const auto flow_config_content = filesystem->read(flow_config_path);
  if (!flow_config_content) {
    throw std::runtime_error(minifi::utils::string::join_pack("Could not read the flow configuration file \"", flow_config_path.string(), "\""));
  }

  const auto process_group = adaptive_configuration.getRootFromPayload(*flow_config_content);
  gsl_Expects(process_group);
  const auto sensitive_properties = listSensitiveProperties(*process_group);

  std::unordered_map<minifi::utils::Identifier, std::unordered_map<std::string, std::string>> overrides = create_overrides(sensitive_properties);
  if (overrides.empty()) {
    return;
  }

  std::string flow_config_str = adaptive_configuration.serializeWithOverrides(*process_group, overrides);
  adaptive_configuration.persist(flow_config_str);
}
}  // namespace

namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor {

void encryptSensitiveValuesInFlowConfig(const EncryptionKeys& keys, const std::filesystem::path& minifi_home, const std::filesystem::path& flow_config_path) {
  encryptSensitiveValuesInFlowConfigImpl(keys, minifi_home, flow_config_path,
      [](const auto& sensitive_properties) {
    std::unordered_map<utils::Identifier, std::unordered_map<std::string, std::string>> overrides;
    std::cout << '\n';
    for (const auto& sensitive_property : sensitive_properties) {
      std::cout << magic_enum::enum_name(sensitive_property.type) << " " << sensitive_property.component_name << " (" << sensitive_property.component_id.to_string() << ") "
          << "has sensitive property " << sensitive_property.property_display_name << "\n    enter a new value or press Enter to keep the current value unchanged: ";
      std::cout.flush();
      std::string new_value;
      std::getline(std::cin, new_value);
      if (!new_value.empty()) {
        overrides[sensitive_property.component_id].emplace(sensitive_property.property_name, new_value);
      }
    }
    return overrides;
  });
}

void encryptSensitiveValuesInFlowConfig(const EncryptionKeys& keys, const std::filesystem::path& minifi_home, const std::filesystem::path& flow_config_path,
    const std::string& component_id, const std::string& property_name, const std::string& property_value) {
  encryptSensitiveValuesInFlowConfigImpl(keys, minifi_home, flow_config_path,
      [&](const auto& sensitive_properties) -> std::unordered_map<utils::Identifier, std::unordered_map<std::string, std::string>> {
    const auto sensitive_property_it = std::ranges::find_if(sensitive_properties, [&](const auto& sensitive_property) {
      return sensitive_property.component_id.to_string() == component_id && (sensitive_property.property_name == property_name || sensitive_property.property_display_name == property_name);
    });
    if (sensitive_property_it == sensitive_properties.end()) {
      std::cout << "No sensitive property found with this component ID and property name.\n";
      return {};
    }
    return {{sensitive_property_it->component_id, {{sensitive_property_it->property_name, property_value}}}};
  });
}

}  // namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor
