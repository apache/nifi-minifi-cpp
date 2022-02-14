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
#include "utils/file/FileUtils.h"
#include "Defaults.h"

namespace {

constexpr const char* OLD_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.key.old";
constexpr const char* ENCRYPTION_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.key";

}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

EncryptConfig::EncryptConfig(const std::string& minifi_home) : minifi_home_(minifi_home) {
  if (sodium_init() < 0) {
    throw std::runtime_error{"Could not initialize the libsodium library!"};
  }
  // encryption/decryption depends on the libsodium library which needs to be initialized
  keys_ = getEncryptionKeys();
}

EncryptConfig::EncryptionType EncryptConfig::encryptSensitiveProperties() const {
  encryptSensitiveProperties(keys_);
  if (keys_.old_key) {
    return EncryptionType::RE_ENCRYPT;
  }
  return EncryptionType::ENCRYPT;
}

void EncryptConfig::encryptFlowConfig() const {
  encrypt_config::ConfigFile properties_file{std::ifstream{propertiesFilePath()}};
  std::optional<std::string> config_path = properties_file.getValue(Configure::nifi_flow_configuration_file);
  if (!config_path) {
    config_path = utils::file::PathUtils::resolve(minifi_home_, "conf/config.yml");
    std::cout << "Couldn't find path of configuration file, using default: \"" << *config_path << "\"\n";
  } else {
    config_path = utils::file::PathUtils::resolve(minifi_home_, *config_path);
    std::cout << "Encrypting flow configuration file: \"" << *config_path << "\"\n";
  }
  std::string config_content;
  try {
    std::ifstream config_file{*config_path, std::ios::binary};
    config_file.exceptions(std::ios::failbit | std::ios::badbit);
    config_content = std::string{std::istreambuf_iterator<char>(config_file), {}};
  } catch (...) {
    throw std::runtime_error("Error while reading flow configuration file \"" + *config_path + "\"");
  }
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
    std::ofstream encrypted_file{*config_path, std::ios::binary};
    encrypted_file.exceptions(std::ios::failbit | std::ios::badbit);
    encrypted_file << encrypted_content;
  } catch (...) {
    throw std::runtime_error("Error while writing encrypted flow configuration file \"" + *config_path + "\"");
  }
  std::cout << "Successfully encrypted flow configuration file: \"" << *config_path << "\"\n";
}

std::string EncryptConfig::bootstrapFilePath() const {
  return utils::file::concat_path(minifi_home_, DEFAULT_BOOTSTRAP_FILE);
}

std::string EncryptConfig::propertiesFilePath() const {
  return utils::file::concat_path(minifi_home_, DEFAULT_NIFI_PROPERTIES_FILE);
}

EncryptionKeys EncryptConfig::getEncryptionKeys() const {
  encrypt_config::ConfigFile bootstrap_file{std::ifstream{bootstrapFilePath()}};
  std::optional<std::string> decryption_key_hex = bootstrap_file.getValue(OLD_KEY_PROPERTY_NAME);
  std::optional<std::string> encryption_key_hex = bootstrap_file.getValue(ENCRYPTION_KEY_PROPERTY_NAME);

  EncryptionKeys keys;
  if (decryption_key_hex && !decryption_key_hex->empty()) {
    std::string binary_key = hexDecodeAndValidateKey(*decryption_key_hex, OLD_KEY_PROPERTY_NAME);
    std::cout << "Old encryption key found in " << bootstrapFilePath() << "\n";
    keys.old_key = utils::crypto::stringToBytes(binary_key);
  }

  if (encryption_key_hex && !encryption_key_hex->empty()) {
    std::string binary_key = hexDecodeAndValidateKey(*encryption_key_hex, ENCRYPTION_KEY_PROPERTY_NAME);
    std::cout << "Using the existing encryption key found in " << bootstrapFilePath() << '\n';
    keys.encryption_key = utils::crypto::stringToBytes(binary_key);
  } else {
    std::cout << "Generating a new encryption key...\n";
    utils::crypto::Bytes encryption_key = utils::crypto::generateKey();
    writeEncryptionKeyToBootstrapFile(encryption_key);
    std::cout << "Wrote the new encryption key to " << bootstrapFilePath() << '\n';
    keys.encryption_key = encryption_key;
  }
  return keys;
}

std::string EncryptConfig::hexDecodeAndValidateKey(const std::string& key, const std::string& key_name) const {
  // Note: from_hex() allows [and skips] non-hex characters
  std::string binary_key = utils::StringUtils::from_hex(key, utils::as_string);
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

void EncryptConfig::writeEncryptionKeyToBootstrapFile(const utils::crypto::Bytes& encryption_key) const {
  std::string key_encoded = utils::StringUtils::to_hex(utils::crypto::bytesToString(encryption_key));
  encrypt_config::ConfigFile bootstrap_file{std::ifstream{bootstrapFilePath()}};

  if (bootstrap_file.hasValue(ENCRYPTION_KEY_PROPERTY_NAME)) {
    bootstrap_file.update(ENCRYPTION_KEY_PROPERTY_NAME, key_encoded);
  } else {
    bootstrap_file.append(ENCRYPTION_KEY_PROPERTY_NAME, key_encoded);
  }

  bootstrap_file.writeTo(bootstrapFilePath());
}

void EncryptConfig::encryptSensitiveProperties(const EncryptionKeys& keys) const {
  encrypt_config::ConfigFile properties_file{std::ifstream{propertiesFilePath()}};
  if (properties_file.size() == 0) {
    throw std::runtime_error{"Properties file " + propertiesFilePath() + " not found!"};
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

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
