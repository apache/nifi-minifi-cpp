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

#include "../core-framework/include/Defaults.h"
#include "ConfigFile.h"
#include "ConfigFileEncryptor.h"
#include "FlowConfigEncryptor.h"
#include "utils/Enum.h"
#include "utils/file/FileUtils.h"

namespace {
constexpr std::string_view ENCRYPTION_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.key";
constexpr std::string_view SENSITIVE_PROPERTIES_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.properties.key";

std::string readFile(const std::filesystem::path& file_path) {
  try {
    std::ifstream file_stream{file_path, std::ios::binary};
    file_stream.exceptions(std::ios::failbit | std::ios::badbit);
    return {std::istreambuf_iterator<char>(file_stream), {}};
  } catch (...) {
    throw std::runtime_error("Error while reading file \"" + file_path.string() + "\"");
  }
}
}  // namespace

namespace org::apache::nifi::minifi::encrypt_config {

EncryptConfig::EncryptConfig(const std::string& minifi_home) : minifi_home_(minifi_home) {
  if (sodium_init() < 0) {
    // encryption/decryption depends on the libsodium library which needs to be initialized
    throw std::runtime_error{"Could not initialize the libsodium library!"};
  }

  std::filesystem::current_path(minifi_home_);
}

bool EncryptConfig::isReEncrypting() const {
  encrypt_config::ConfigFile bootstrap_file{std::ifstream{bootstrapFilePath()}};

  std::string decryption_key_name = utils::string::join_pack(ENCRYPTION_KEY_PROPERTY_NAME, ".old");
  std::optional<std::string> decryption_key_hex = bootstrap_file.getValue(decryption_key_name);

  return (decryption_key_hex && !decryption_key_hex->empty());
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

void EncryptConfig::encryptWholeFlowConfigFile() const {
  EncryptionKeys keys = getEncryptionKeys(ENCRYPTION_KEY_PROPERTY_NAME);

  std::filesystem::path config_path = flowConfigPath();
  std::string config_content = readFile(config_path);
  try {
    utils::crypto::decrypt(config_content, keys.encryption_key);
    std::cout << "Flow config file is already properly encrypted.\n";
    return;
  } catch (const std::exception&) {}

  if (utils::crypto::isEncrypted(config_content)) {
    if (!keys.old_key) {
      throw std::runtime_error("Config file is encrypted, but no old encryption key is set.");
    }
    std::cout << "Trying to decrypt flow config file using the old key ...\n";
    try {
      config_content = utils::crypto::decrypt(config_content, *keys.old_key);
    } catch (const std::exception&) {
      throw std::runtime_error("Flow config is encrypted, but couldn't be decrypted.");
    }
  } else {
    std::cout << "Flow config file is not encrypted, using as-is.\n";
  }

  std::string encrypted_content = utils::crypto::encrypt(config_content, keys.encryption_key);
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

void EncryptConfig::encryptSensitiveValuesInMinifiProperties() const {
  EncryptionKeys keys = getEncryptionKeys(ENCRYPTION_KEY_PROPERTY_NAME);

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

void EncryptConfig::encryptSensitiveValuesInFlowConfig(
    bool re_encrypt, const std::optional<std::string>& component_id, const std::optional<std::string>& item_name, const std::optional<std::string>& item_value) const {
  EncryptionKeys keys = getEncryptionKeys(SENSITIVE_PROPERTIES_KEY_PROPERTY_NAME);
  flow_config_encryptor::EncryptionRequest request_type = [&] {
    if (re_encrypt) {
      return flow_config_encryptor::EncryptionRequest{flow_config_encryptor::EncryptionType::ReEncrypt};
    } else if (!component_id && !item_name && !item_value) {
      return flow_config_encryptor::EncryptionRequest{flow_config_encryptor::EncryptionType::Interactive};
    } else if (component_id && item_name && item_value) {
      return flow_config_encryptor::EncryptionRequest{*component_id, *item_name, *item_value};
    } else {
      throw std::runtime_error("either all of --component-id, --property-name and --property-value should be given (for batch mode) or none of them (for interactive mode)");
    }
  }();
  flow_config_encryptor::encryptSensitiveValuesInFlowConfig(keys, minifi_home_, flowConfigPath(), request_type);
}

}  // namespace org::apache::nifi::minifi::encrypt_config
