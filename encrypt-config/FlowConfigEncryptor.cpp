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
enum class ComponentType {
  Processor,
  ControllerService
};

struct SensitiveProperty {
  ComponentType component_type;
  minifi::utils::Identifier component_id;
  std::string component_name;
  std::string property_name;
  std::string property_display_name;
  std::string property_value;
};
}  // namespace

namespace magic_enum::customize {
template<>
constexpr customize_t enum_name<ComponentType>(ComponentType type) noexcept {
  switch (type) {
    case ComponentType::Processor: return "Processor";
    case ComponentType::ControllerService: return "Controller service";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace {
std::vector<SensitiveProperty> listSensitiveProperties(const minifi::core::ProcessGroup &process_group) {
  std::vector<SensitiveProperty> sensitive_properties;

  std::vector<minifi::core::Processor *> processors;
  process_group.getAllProcessors(processors);
  for (const auto* processor : processors) {
    gsl_Expects(processor);
    for (const auto& [_, property] : processor->getProperties()) {
      if (property.isSensitive()) {
        sensitive_properties.push_back(SensitiveProperty{
            .component_type = ComponentType::Processor,
            .component_id = processor->getUUID(),
            .component_name = processor->getName(),
            .property_name = property.getName(),
            .property_display_name = property.getDisplayName(),
            .property_value = property.getValue().to_string()});
      }
    }
  }

  for (const auto* controller_service_node : process_group.getAllControllerServices()) {
    gsl_Expects(controller_service_node);
    const auto* controller_service = controller_service_node->getControllerServiceImplementation();
    gsl_Expects(controller_service);
    for (const auto& [_, property] : controller_service->getProperties()) {
      if (property.isSensitive()) {
        sensitive_properties.push_back(SensitiveProperty{
            .component_type = ComponentType::ControllerService,
            .component_id = controller_service->getUUID(),
            .component_name = controller_service->getName(),
            .property_name = property.getName(),
            .property_display_name = property.getDisplayName(),
            .property_value = property.getValue().to_string()});
      }
    }
  }

  return sensitive_properties;
}

std::unordered_map<minifi::utils::Identifier, std::unordered_map<std::string, std::string>> createOverridesInteractively(const std::vector<SensitiveProperty>& sensitive_properties) {
  std::unordered_map<minifi::utils::Identifier, std::unordered_map<std::string, std::string>> overrides;
  std::cout << '\n';
  for (const auto& sensitive_property : sensitive_properties) {
    std::cout << magic_enum::enum_name(sensitive_property.component_type) << " " << sensitive_property.component_name << " (" << sensitive_property.component_id.to_string() << ") "
              << "has sensitive property " << sensitive_property.property_display_name << "\n    enter a new value or press Enter to keep the current value unchanged: ";
    std::cout.flush();
    std::string new_value;
    std::getline(std::cin, new_value);
    if (!new_value.empty()) {
      overrides[sensitive_property.component_id].emplace(sensitive_property.property_name, new_value);
    }
  }
  return overrides;
}

std::unordered_map<minifi::utils::Identifier, std::unordered_map<std::string, std::string>> createOverridesForSingleProperty(
    const std::vector<SensitiveProperty>& sensitive_properties, const std::string& component_id, const std::string& property_name, const std::string& property_value) {
  const auto sensitive_property_it = std::ranges::find_if(sensitive_properties, [&](const auto& sensitive_property) {
    return sensitive_property.component_id.to_string().view() == component_id && (sensitive_property.property_name == property_name || sensitive_property.property_display_name == property_name);
  });
  if (sensitive_property_it == sensitive_properties.end()) {
    std::cout << "No sensitive property found with this component ID and property name.\n";
    return {};
  }
  return {{sensitive_property_it->component_id, {{sensitive_property_it->property_name, property_value}}}};
}

std::unordered_map<minifi::utils::Identifier, std::unordered_map<std::string, std::string>> createOverridesForReEncryption(const std::vector<SensitiveProperty>& sensitive_properties) {
  std::unordered_map<minifi::utils::Identifier, std::unordered_map<std::string, std::string>> overrides;
  for (const auto& sensitive_property : sensitive_properties) {
    overrides[sensitive_property.component_id].emplace(sensitive_property.property_name, sensitive_property.property_value);
  }
  return overrides;
}

}  // namespace

namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor {

EncryptionRequest::EncryptionRequest(EncryptionType type) : type{type} {
  gsl_Expects(type == EncryptionType::Interactive || type == EncryptionType::ReEncrypt);
}

EncryptionRequest::EncryptionRequest(std::string_view component_id, std::string_view property_name, std::string_view property_value)
    : type{EncryptionType::SingleProperty},
      component_id{component_id},
      property_name{property_name},
      property_value{property_value} {}

void encryptSensitiveValuesInFlowConfig(const EncryptionKeys& keys, const std::filesystem::path& minifi_home, const std::filesystem::path& flow_config_path, const EncryptionRequest& request) {
  const bool is_re_encrypting = keys.old_key.has_value();
  if (is_re_encrypting && request.type != EncryptionType::ReEncrypt) {
    throw std::runtime_error("Error: found .old key; please run --re-encrypt and then remove the .old key");
  }
  if (!is_re_encrypting && request.type == EncryptionType::ReEncrypt) {
    throw std::runtime_error("Error: cannot re-encrypt without an .old key!");
  }

  const auto configure = std::make_shared<Configure>();
  configure->setHome(minifi_home);
  configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  bool encrypt_whole_flow_config_file = (configure->get(Configure::nifi_flow_configuration_encrypt) | utils::andThen(utils::string::toBool)).value_or(false);
  auto whole_file_encryptor = encrypt_whole_flow_config_file ? utils::crypto::EncryptionProvider::create(minifi_home) : std::nullopt;
  auto filesystem = std::make_shared<utils::file::FileSystem>(encrypt_whole_flow_config_file, whole_file_encryptor);

  auto sensitive_properties_encryptor = is_re_encrypting ?
      utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{*keys.old_key}} :
      utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{keys.encryption_key}};

  core::extension::ExtensionManager::get().initialize(configure);

  core::flow::AdaptiveConfiguration adaptive_configuration{core::ConfigurationContext{
      .flow_file_repo = nullptr,
      .content_repo = nullptr,
      .configuration = configure,
      .path = flow_config_path,
      .filesystem = filesystem,
      .sensitive_properties_encryptor = sensitive_properties_encryptor
  }};

  const auto flow_config_content = filesystem->read(flow_config_path);
  if (!flow_config_content) {
    throw std::runtime_error(utils::string::join_pack("Could not read the flow configuration file \"", flow_config_path.string(), "\""));
  }

  const auto process_group = adaptive_configuration.getRootFromPayload(*flow_config_content);
  gsl_Expects(process_group);
  const auto sensitive_properties = listSensitiveProperties(*process_group);

  auto overrides = [&]() -> std::unordered_map<utils::Identifier, std::unordered_map<std::string, std::string>> {
    switch (request.type) {
      case EncryptionType::Interactive: return createOverridesInteractively(sensitive_properties);
      case EncryptionType::SingleProperty: return createOverridesForSingleProperty(sensitive_properties, request.component_id, request.property_name, request.property_value);
      case EncryptionType::ReEncrypt: return createOverridesForReEncryption(sensitive_properties);
    }
    return {};
  }();
  if (overrides.empty()) {
    return;
  }

  if (is_re_encrypting) {
    adaptive_configuration.setSensitivePropertiesEncryptor(utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{keys.encryption_key}});
  }

  std::string flow_config_str = adaptive_configuration.serializeWithOverrides(*process_group, overrides);
  adaptive_configuration.persist(flow_config_str);
}

}  // namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor
