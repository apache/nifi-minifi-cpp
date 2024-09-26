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
  ControllerService,
  ParameterContext
};

struct SensitiveItem {
  ComponentType component_type;
  minifi::utils::Identifier component_id;
  std::string component_name;
  std::string item_name;
  std::string item_display_name;
  std::string item_value;
};

}  // namespace

namespace magic_enum::customize {
template<>
constexpr customize_t enum_name<ComponentType>(ComponentType type) noexcept {
  switch (type) {
    case ComponentType::Processor: return "Processor";
    case ComponentType::ControllerService: return "Controller service";
    case ComponentType::ParameterContext: return "Parameter context";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace {
std::vector<SensitiveItem> listSensitiveItems(const minifi::core::ProcessGroup &process_group,
    const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<minifi::core::ParameterContext>>>& parameter_contexts) {
  std::vector<SensitiveItem> sensitive_items;

  for (const auto& [parameter_context_name, parameter_context] : parameter_contexts) {
    for (const auto& [parameter_name, parameter] : parameter_context->getParameters()) {
      if (parameter.sensitive) {
        sensitive_items.push_back(SensitiveItem{
            .component_type = ComponentType::ParameterContext,
            .component_id = parameter_context->getUUID(),
            .component_name = parameter_context_name,
            .item_name = parameter_name,
            .item_display_name = parameter_name,
            .item_value = parameter.value});
      }
    }
  }

  std::vector<minifi::core::Processor *> processors;
  process_group.getAllProcessors(processors);
  for (const auto* processor : processors) {
    gsl_Expects(processor);
    for (const auto& [_, property] : processor->getProperties()) {
      if (property.isSensitive()) {
        sensitive_items.push_back(SensitiveItem{
            .component_type = ComponentType::Processor,
            .component_id = processor->getUUID(),
            .component_name = processor->getName(),
            .item_name = property.getName(),
            .item_display_name = property.getDisplayName(),
            .item_value = property.getValue().to_string()});
      }
    }
  }

  std::unordered_set<minifi::utils::Identifier> processed_controller_services;
  for (const auto* controller_service_node : process_group.getAllControllerServices()) {
    gsl_Expects(controller_service_node);
    const auto* controller_service = controller_service_node->getControllerServiceImplementation();
    gsl_Expects(controller_service);
    if (processed_controller_services.contains(controller_service->getUUID())) {
      continue;
    }
    processed_controller_services.insert(controller_service->getUUID());
    for (const auto& [_, property] : controller_service->getProperties()) {
      if (property.isSensitive()) {
        sensitive_items.push_back(SensitiveItem{
            .component_type = ComponentType::ControllerService,
            .component_id = controller_service->getUUID(),
            .component_name = controller_service->getName(),
            .item_name = property.getName(),
            .item_display_name = property.getDisplayName(),
            .item_value = property.getValue().to_string()});
      }
    }
  }

  return sensitive_items;
}

std::unordered_map<minifi::utils::Identifier, minifi::core::flow::Overrides> createOverridesInteractively(const std::vector<SensitiveItem>& sensitive_items) {
  std::unordered_map<minifi::utils::Identifier, minifi::core::flow::Overrides> overrides;
  std::cout << '\n';
  for (const auto& sensitive_item : sensitive_items) {
    std::cout << magic_enum::enum_name(sensitive_item.component_type) << " " << sensitive_item.component_name << " (" << sensitive_item.component_id.to_string() << ") "
              << "has sensitive property or parameter " << sensitive_item.item_display_name << "\n    enter a new value or press Enter to keep the current value unchanged: ";
    std::cout.flush();
    std::string new_value;
    std::getline(std::cin, new_value);
    if (!new_value.empty()) {
      overrides[sensitive_item.component_id].add(sensitive_item.item_name, new_value);
    }
  }
  return overrides;
}

std::unordered_map<minifi::utils::Identifier, minifi::core::flow::Overrides> createOverridesForSingleItem(
    const std::vector<SensitiveItem>& sensitive_items, const std::string& component_id, const std::string& item_name, const std::string& item_value) {
  const auto sensitive_item_it = std::ranges::find_if(sensitive_items, [&](const auto& sensitive_item) {
    return sensitive_item.component_id.to_string().view() == component_id && (sensitive_item.item_name == item_name || sensitive_item.item_display_name == item_name);
  });
  if (sensitive_item_it == sensitive_items.end()) {
    std::cout << "No sensitive property or parameter found with this component ID and name.\n";
    return {};
  }
  return {{sensitive_item_it->component_id, minifi::core::flow::Overrides{}.add(sensitive_item_it->item_name, item_value)}};
}

std::unordered_map<minifi::utils::Identifier, minifi::core::flow::Overrides> createOverridesForReEncryption(const std::vector<SensitiveItem>& sensitive_items) {
  std::unordered_map<minifi::utils::Identifier, minifi::core::flow::Overrides> overrides;
  for (const auto& sensitive_item : sensitive_items) {
    overrides[sensitive_item.component_id].addOptional(sensitive_item.item_name, sensitive_item.item_value);
  }
  return overrides;
}

}  // namespace

namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor {

EncryptionRequest::EncryptionRequest(EncryptionType type) : type{type} {
  gsl_Expects(type == EncryptionType::Interactive || type == EncryptionType::ReEncrypt);
}

EncryptionRequest::EncryptionRequest(std::string_view component_id, std::string_view item_name, std::string_view item_value)
    : type{EncryptionType::SingleProperty},
      component_id{component_id},
      item_name{item_name},
      item_value{item_value} {}

void encryptSensitiveValuesInFlowConfig(const EncryptionKeys& keys, const std::filesystem::path& minifi_home, const std::filesystem::path& flow_config_path, const EncryptionRequest& request) {
  const bool is_re_encrypting = keys.old_key.has_value();
  if (is_re_encrypting && request.type != EncryptionType::ReEncrypt) {
    throw std::runtime_error("Error: found .old key; please run --re-encrypt and then remove the .old key");
  }
  if (!is_re_encrypting && request.type == EncryptionType::ReEncrypt) {
    throw std::runtime_error("Error: cannot re-encrypt without an .old key!");
  }

  const auto configure = std::make_shared<ConfigureImpl>();
  configure->setHome(minifi_home);
  configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  bool encrypt_whole_flow_config_file = (configure->get(Configure::nifi_flow_configuration_encrypt) | utils::andThen(utils::string::toBool)).value_or(false);
  auto whole_file_encryptor = encrypt_whole_flow_config_file ? utils::crypto::EncryptionProvider::create(minifi_home) : std::nullopt;
  auto filesystem = std::make_shared<utils::file::FileSystem>(encrypt_whole_flow_config_file, whole_file_encryptor);

  auto sensitive_values_decryptor = is_re_encrypting ?
      utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{*keys.old_key}} :
      utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{keys.encryption_key}};

  core::extension::ExtensionManagerImpl::get().initialize(configure);

  core::flow::AdaptiveConfiguration adaptive_configuration{core::ConfigurationContext{
      .flow_file_repo = nullptr,
      .content_repo = nullptr,
      .configuration = configure,
      .path = flow_config_path,
      .filesystem = filesystem,
      .sensitive_values_encryptor = sensitive_values_decryptor
  }};

  const auto flow_config_content = filesystem->read(flow_config_path);
  if (!flow_config_content) {
    throw std::runtime_error(utils::string::join_pack("Could not read the flow configuration file \"", flow_config_path.string(), "\""));
  }

  const auto process_group = adaptive_configuration.getRootFromPayload(*flow_config_content);
  gsl_Expects(process_group);
  const auto sensitive_items = listSensitiveItems(*process_group, adaptive_configuration.getParameterContexts());

  const auto overrides = [&]() -> std::unordered_map<utils::Identifier, core::flow::Overrides> {
    switch (request.type) {
      case EncryptionType::Interactive: return createOverridesInteractively(sensitive_items);
      case EncryptionType::SingleProperty: return createOverridesForSingleItem(sensitive_items, request.component_id, request.item_name, request.item_value);
      case EncryptionType::ReEncrypt: return createOverridesForReEncryption(sensitive_items);
    }
    return {};
  }();

  if (overrides.empty()) {
    std::cout << "Nothing to do, exiting.\n";
    return;
  }

  if (is_re_encrypting) {
    adaptive_configuration.setSensitivePropertiesEncryptor(utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{keys.encryption_key}});
  }

  std::string flow_config_str = adaptive_configuration.serializeWithOverrides(*process_group, overrides);
  adaptive_configuration.persist(flow_config_str);
}

}  // namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor
