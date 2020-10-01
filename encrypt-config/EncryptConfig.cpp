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

#include <stdexcept>

#include "ConfigFile.h"
#include "ConfigFileEncryptor.h"
#include "cxxopts.hpp"
#include "utils/file/FileUtils.h"
#include "utils/OptionalUtils.h"

namespace {

constexpr const char* CONF_DIRECTORY_NAME = "conf";
constexpr const char* BOOTSTRAP_FILE_NAME = "bootstrap.conf";
constexpr const char* MINIFI_PROPERTIES_FILE_NAME = "minifi.properties";
constexpr const char* ENCRYPTION_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.key";

}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

EncryptConfig::EncryptConfig(int argc, char* argv[]) : minifi_home_(parseMinifiHomeFromTheOptions(argc, argv)) {
  if (sodium_init() < 0) {
    throw std::runtime_error{"Could not initialize the libsodium library!"};
  }
}

std::string EncryptConfig::parseMinifiHomeFromTheOptions(int argc, char* argv[]) {
  cxxopts::Options options("encrypt-config", "Encrypt sensitive minifi properties");
  options.add_options()
      ("h,help", "Shows help")
      ("m,minifi-home", "The MINIFI_HOME directory", cxxopts::value<std::string>());

  auto parse_result = options.parse(argc, argv);

  if (parse_result.count("help")) {
    std::cout << options.help() << '\n';
    std::exit(0);
  }

  if (parse_result.count("minifi-home")) {
    return parse_result["minifi-home"].as<std::string>();
  } else {
    throw std::runtime_error{"Required parameter missing: --minifi-home"};
  }
}

void EncryptConfig::encryptSensitiveProperties() const {
  utils::crypto::Bytes encryption_key = getEncryptionKey();
  encryptSensitiveProperties(encryption_key);
}

std::string EncryptConfig::bootstrapFilePath() const {
  return utils::file::concat_path(
      utils::file::concat_path(minifi_home_, CONF_DIRECTORY_NAME),
      BOOTSTRAP_FILE_NAME);
}

std::string EncryptConfig::propertiesFilePath() const {
  return utils::file::concat_path(
      utils::file::concat_path(minifi_home_, CONF_DIRECTORY_NAME),
      MINIFI_PROPERTIES_FILE_NAME);
}

utils::crypto::Bytes EncryptConfig::getEncryptionKey() const {
  encrypt_config::ConfigFile bootstrap_file{std::ifstream{bootstrapFilePath()}};
  utils::optional<std::string> key_from_bootstrap_file = bootstrap_file.getValue(ENCRYPTION_KEY_PROPERTY_NAME);

  if (key_from_bootstrap_file && !key_from_bootstrap_file->empty()) {
    std::string binary_key = hexDecodeAndValidateKey(*key_from_bootstrap_file);
    std::cout << "Using the existing encryption key found in " << bootstrapFilePath() << '\n';
    return utils::crypto::stringToBytes(binary_key);
  } else {
    std::cout << "Generating a new encryption key...\n";
    utils::crypto::Bytes encryption_key = utils::crypto::generateKey();
    writeEncryptionKeyToBootstrapFile(encryption_key);
    std::cout << "Wrote the new encryption key to " << bootstrapFilePath() << '\n';
    return encryption_key;
  }
}

std::string EncryptConfig::hexDecodeAndValidateKey(const std::string& key) const {
  // Note: from_hex() allows [and skips] non-hex characters
  std::string binary_key = utils::StringUtils::from_hex(key);
  if (binary_key.size() == utils::crypto::EncryptionType::keyLength()) {
    return binary_key;
  } else {
    std::stringstream error;
    error << "The encryption key " << ENCRYPTION_KEY_PROPERTY_NAME << " in the bootstrap file\n"
        << "    " << bootstrapFilePath() << '\n'
        << "is invalid; delete it to generate a new key.";
    throw std::runtime_error{error.str()};
  }
}

void EncryptConfig::writeEncryptionKeyToBootstrapFile(const utils::crypto::Bytes& encryption_key) const {
  std::string key_encoded = utils::StringUtils::to_hex(utils::crypto::bytesToString(encryption_key));
  encrypt_config::ConfigFile bootstrap_file{std::ifstream{bootstrapFilePath()}};

  if (bootstrap_file.getValue(ENCRYPTION_KEY_PROPERTY_NAME)) {
    bootstrap_file.update(ENCRYPTION_KEY_PROPERTY_NAME, key_encoded);
  } else {
    bootstrap_file.append(ENCRYPTION_KEY_PROPERTY_NAME, key_encoded);
  }

  bootstrap_file.writeTo(bootstrapFilePath());
}

void EncryptConfig::encryptSensitiveProperties(const utils::crypto::Bytes& encryption_key) const {
  encrypt_config::ConfigFile properties_file{std::ifstream{propertiesFilePath()}};
  if (properties_file.size() == 0) {
    throw std::runtime_error{"Properties file " + propertiesFilePath() + " not found!"};
  }

  uint32_t num_properties_encrypted = encryptSensitivePropertiesInFile(properties_file, encryption_key);
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
