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

#include "EncryptConfig.h"

#include <sodium.h>

#include <fstream>
#include <optional>
#include <stdexcept>

#include "ConfigFile.h"
#include "ConfigFileEncryptor.h"
#include "core/flow/AdaptiveConfiguration.h"
#include "core/FlowConfiguration.h"
#include "core/RepositoryFactory.h"
#include "core/repository/VolatileContentRepository.h"
#include "utils/Enum.h"
#include "utils/file/FileUtils.h"
#include "Defaults.h"
#include "core/extension/ExtensionManager.h"

namespace {
namespace minifi = org::apache::nifi::minifi;

constexpr std::string_view ENCRYPTION_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.key";
constexpr std::string_view SENSITIVE_PROPERTIES_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.properties.key";

enum class Type {
  Processor, ControllerService
};

struct SensitiveProperty {
  Type type;
  minifi::utils::Identifier id;
  std::string name;
  std::string property_name;
  std::string property_display_name;
};

std::vector<SensitiveProperty> listSensitiveProperties(const minifi::core::ProcessGroup& process_group) {
  std::vector<SensitiveProperty> sensitive_properties;

  std::vector<minifi::core::Processor*> processors;
  process_group.getAllProcessors(processors);
  for (const auto* processor : processors) {
    gsl_Expects(processor);
    for (const auto& [_, property] : processor->getProperties()) {
      if (property.isSensitive()) {
        sensitive_properties.push_back(SensitiveProperty{Type::Processor, processor->getUUID(), processor->getName(), property.getName(), property.getDisplayName()});
      }
    }
  }

  for (const auto& controller_service_node : process_group.getAllControllerServices()) {
    gsl_Expects(controller_service_node);
    const auto controller_service = controller_service_node->getControllerServiceImplementation();
    gsl_Expects(controller_service);
    for (const auto& [_, property] : controller_service->getProperties()) {
      if (property.isSensitive()) {
        sensitive_properties.push_back(SensitiveProperty{Type::ControllerService, controller_service->getUUID(), controller_service->getName(), property.getName(), property.getDisplayName()});
      }
    }
  }

  return sensitive_properties;
}
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

namespace org::apache::nifi::minifi::encrypt_config {

EncryptConfig::EncryptConfig(const std::string& minifi_home) : minifi_home_(minifi_home) {
  if (sodium_init() < 0) {
    // encryption/decryption depends on the libsodium library which needs to be initialized
    throw std::runtime_error{"Could not initialize the libsodium library!"};
  }

  std::filesystem::current_path(minifi_home_);

  keys_ = getEncryptionKeys(ENCRYPTION_KEY_PROPERTY_NAME);
  sensitive_properties_keys_ = getEncryptionKeys(SENSITIVE_PROPERTIES_KEY_PROPERTY_NAME);
}

EncryptConfig::EncryptionType EncryptConfig::encryptionType() const {
  return keys_.old_key ? EncryptionType::RE_ENCRYPT : EncryptionType::ENCRYPT;
}

void EncryptConfig::encryptSensitiveValuesInMinifiProperties() const {
  encryptSensitiveValuesInMinifiProperties(keys_);
}

void EncryptConfig::encryptSensitiveValuesInFlowConfig() const {
  encryptSensitiveValuesInFlowConfig(sensitive_properties_keys_);
}

std::filesystem::path EncryptConfig::flowConfigPath() const {
  encrypt_config::ConfigFile properties_file{std::ifstream{propertiesFilePath()}};
  std::optional<std::filesystem::path> config_path{properties_file.getValue(Configure::nifi_flow_configuration_file)};
  if (!config_path) {
    config_path = utils::file::PathUtils::resolve(minifi_home_, "conf/config.yml");
    std::cout << "Couldn't find path of configuration file, using default: " << *config_path << '\n';
  } else {
    config_path = utils::file::PathUtils::resolve(minifi_home_, *config_path);
    std::cout << "Encrypting flow configuration file: " << *config_path << '\n';
  }
  return *config_path;
}

std::string EncryptConfig::flowConfigContent(const std::filesystem::path& config_path) {
  std::string config_content;
  try {
    std::ifstream config_file{config_path, std::ios::binary};
    config_file.exceptions(std::ios::failbit | std::ios::badbit);
    return {std::istreambuf_iterator<char>(config_file), {}};
  } catch (...) {
    throw std::runtime_error("Error while reading flow configuration file \"" + config_path.string() + "\"");
  }
}

void EncryptConfig::encryptFlowConfigBlob() const {
  std::filesystem::path config_path = flowConfigPath();
  std::string config_content = flowConfigContent(config_path);
  try {
    utils::crypto::decrypt(config_content, keys_.encryption_key);
    std::cout << "Flow config file is already properly encrypted.\n";
    return;
  } catch (const std::exception&) {}

  if (utils::crypto::isEncrypted(config_content)) {
    if (!keys_.old_key) {
      throw std::runtime_error("Config file is encrypted, but no old encryption key is set.");
    }
    std::cout << "Trying to decrypt flow config file using the old key ...\n";
    try {
      config_content = utils::crypto::decrypt(config_content, *keys_.old_key);
    } catch (const std::exception&) {
      throw std::runtime_error("Flow config is encrypted, but couldn't be decrypted.");
    }
  } else {
    std::cout << "Flow config file is not encrypted, using as-is.\n";
  }

  std::string encrypted_content = utils::crypto::encrypt(config_content, keys_.encryption_key);
  try {
    std::ofstream encrypted_file{config_path, std::ios::binary};
    encrypted_file.exceptions(std::ios::failbit | std::ios::badbit);
    encrypted_file << encrypted_content;
  } catch (...) {
    throw std::runtime_error("Error while writing encrypted flow configuration file \"" + config_path.string() + "\"");
  }
  std::cout << "Successfully encrypted flow configuration file: " << config_path << '\n';
}

std::filesystem::path EncryptConfig::bootstrapFilePath() const {
  return minifi_home_ / DEFAULT_BOOTSTRAP_FILE;
}

std::filesystem::path EncryptConfig::propertiesFilePath() const {
  return minifi_home_ / DEFAULT_NIFI_PROPERTIES_FILE;
}

EncryptionKeys EncryptConfig::getEncryptionKeys(std::string_view property_name) const {
  encrypt_config::ConfigFile bootstrap_file{std::ifstream{bootstrapFilePath()}};

  std::string decryption_key_name = utils::string::join_pack(property_name, ".old");
  std::optional<std::string> decryption_key_hex = bootstrap_file.getValue(decryption_key_name);

  std::string encryption_key_name{property_name};
  std::optional<std::string> encryption_key_hex = bootstrap_file.getValue(encryption_key_name);

  EncryptionKeys keys;
  if (decryption_key_hex && !decryption_key_hex->empty()) {
    std::string binary_key = hexDecodeAndValidateKey(*decryption_key_hex, decryption_key_name);
    std::cout << "Old encryption key found in " << bootstrapFilePath() << "\n";
    keys.old_key = utils::crypto::stringToBytes(binary_key);
  }

  if (encryption_key_hex && !encryption_key_hex->empty()) {
    std::string binary_key = hexDecodeAndValidateKey(*encryption_key_hex, encryption_key_name);
    std::cout << "Using the existing encryption key " << property_name << " found in " << bootstrapFilePath() << '\n';
    keys.encryption_key = utils::crypto::stringToBytes(binary_key);
  } else {
    std::cout << "Generating a new encryption key...\n";
    utils::crypto::Bytes encryption_key = utils::crypto::generateKey();
    writeEncryptionKeyToBootstrapFile(encryption_key_name, encryption_key);
    std::cout << "Wrote the new encryption key " << property_name << " to " << bootstrapFilePath() << '\n';
    keys.encryption_key = encryption_key;
  }
  return keys;
}

std::string EncryptConfig::hexDecodeAndValidateKey(const std::string& key, const std::string& key_name) const {
  // Note: from_hex() allows [and skips] non-hex characters
  std::string binary_key = utils::string::from_hex(key, utils::as_string);
  if (binary_key.size() == utils::crypto::EncryptionType::keyLength()) {
    return binary_key;
  } else {
    std::stringstream error;
    error << "The encryption key \"" << key_name << "\" in the bootstrap file\n"
        << "    " << bootstrapFilePath() << '\n'
        << "is invalid.";
    throw std::runtime_error{error.str()};
  }
}

void EncryptConfig::writeEncryptionKeyToBootstrapFile(const std::string& encryption_key_name, const utils::crypto::Bytes& encryption_key) const {
  std::string key_encoded = utils::string::to_hex(utils::crypto::bytesToString(encryption_key));
  encrypt_config::ConfigFile bootstrap_file{std::ifstream{bootstrapFilePath()}};

  if (bootstrap_file.hasValue(encryption_key_name)) {
    bootstrap_file.update(encryption_key_name, key_encoded);
  } else {
    bootstrap_file.append(encryption_key_name, key_encoded);
  }

  bootstrap_file.writeTo(bootstrapFilePath());
}

void EncryptConfig::encryptSensitiveValuesInMinifiProperties(const EncryptionKeys& keys) const {
  encrypt_config::ConfigFile properties_file{std::ifstream{propertiesFilePath()}};
  if (properties_file.size() == 0) {
    throw std::runtime_error{"Properties file " + propertiesFilePath().string() + " not found!"};
  }

  uint32_t num_properties_encrypted = encryptSensitivePropertiesInFile(properties_file, keys);
  if (num_properties_encrypted == 0) {
    std::cout << "Could not find any (new) sensitive properties to encrypt in " << propertiesFilePath() << '\n';
    return;
  }

  properties_file.writeTo(propertiesFilePath());
  std::cout << "Encrypted " << num_properties_encrypted << " sensitive "
      << (num_properties_encrypted == 1 ? "property" : "properties") << " in " << propertiesFilePath() << '\n';
}

void EncryptConfig::encryptSensitiveValuesInFlowConfig(const EncryptionKeys& keys) const {
  const auto configure = std::make_shared<minifi::Configure>();
  configure->setHome(minifi_home_);
  configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  bool bulk_encrypt_flow_config = (configure->get(minifi::Configure::nifi_flow_configuration_encrypt) | utils::andThen(utils::string::toBool)).value_or(false);
  auto encryptor = bulk_encrypt_flow_config ? utils::crypto::EncryptionProvider::create(minifi_home_) : std::nullopt;
  auto filesystem = std::make_shared<utils::file::FileSystem>(bulk_encrypt_flow_config, encryptor);

  minifi::core::extension::ExtensionManager::get().initialize(configure);

  const std::filesystem::path flow_config_path = flowConfigPath();
  const auto configuration_context = core::ConfigurationContext{
      .flow_file_repo = core::createRepository("flowfilerepository"),
      .content_repo = std::make_shared<core::repository::VolatileContentRepository>(),
      .configuration = configure,
      .path = flow_config_path,
      .filesystem = filesystem,
      .sensitive_properties_encryptor = utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{keys.encryption_key}}
  };
  core::flow::AdaptiveConfiguration adaptive_configuration{configuration_context};

  const auto flow_config_content = filesystem->read(flow_config_path);
  if (!flow_config_content) {
    throw std::runtime_error(utils::string::join_pack("Could not read the flow configuration file \"", flow_config_path.string(), "\""));
  }

  const auto process_group = adaptive_configuration.getRootFromPayload(*flow_config_content);
  gsl_Expects(process_group);
  const auto sensitive_properties = listSensitiveProperties(*process_group);

  std::unordered_map<utils::Identifier, std::unordered_map<std::string, std::string>> new_sensitive_property_values;
  std::cout << '\n';
  for (const auto& sensitive_property : sensitive_properties) {
    std::cout << magic_enum::enum_name(sensitive_property.type) << " " << sensitive_property.name << " (" << sensitive_property.id.to_string() << ") "
        << "has sensitive property " << sensitive_property.property_display_name << "\n    enter a new value or press Enter to keep the current value unchanged: ";
    std::cout.flush();
    std::string new_value;
    std::getline(std::cin, new_value);
    if (!new_value.empty()) {
      new_sensitive_property_values[sensitive_property.id].emplace(sensitive_property.property_name, new_value);
    }
  }

  std::string flow_config_str = adaptive_configuration.serializeWithOverrides(*process_group, new_sensitive_property_values);
  adaptive_configuration.persist(flow_config_str);
}

}  // namespace org::apache::nifi::minifi::encrypt_config
