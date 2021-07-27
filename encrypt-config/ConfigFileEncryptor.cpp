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

#include "ConfigFileEncryptor.h"

#include <iostream>
#include <optional>
#include <string>

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

bool isEncrypted(const std::optional<std::string>& encryption_type) {
  return encryption_type && !encryption_type->empty() && *encryption_type  != "plaintext";
}

uint32_t encryptSensitivePropertiesInFile(ConfigFile& config_file, const utils::crypto::Bytes & encryption_key) {
  return encryptSensitivePropertiesInFile(config_file, EncryptionKeys{{}, encryption_key});
}

uint32_t encryptSensitivePropertiesInFile(ConfigFile& config_file, const EncryptionKeys& keys) {
  int num_properties_encrypted = 0;

  for (const auto& property_key : config_file.getSensitiveProperties()) {
    std::optional<std::string> property_value = config_file.getValue(property_key);
    if (!property_value) { continue; }

    std::string encryption_type_key = property_key + ".protected";
    std::optional<std::string> encryption_type = config_file.getValue(encryption_type_key);

    std::string raw_value = *property_value;
    if (isEncrypted(encryption_type)) {
      try {
        utils::crypto::decrypt(raw_value, keys.encryption_key);
        std::cout << "Property \"" << property_key << "\" is already properly encrypted.\n";
        continue;
      } catch (const std::exception&) {}
      if (!keys.old_key) {
        throw std::runtime_error("No old encryption key is provided to attempt decryption of property \"" + property_key + "\"");
      }
      try {
        raw_value = utils::crypto::decrypt(raw_value, *keys.old_key);
        std::cout << "Successfully decrypted property \"" << property_key << "\" using old key.\n";
      } catch (const std::exception&) {
        throw std::runtime_error("Couldn't decrypt property \"" + property_key + "\" using the old key.");
      }
    }

    std::string encrypted_property_value = utils::crypto::encrypt(raw_value, keys.encryption_key);

    config_file.update(property_key, encrypted_property_value);

    if (encryption_type) {
      config_file.update(encryption_type_key, utils::crypto::EncryptionType::name());
    } else {
      config_file.insertAfter(property_key, encryption_type_key, utils::crypto::EncryptionType::name());
    }

    std::cout << "Encrypted property: " << property_key << '\n';
    ++num_properties_encrypted;
  }

  return num_properties_encrypted;
}

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
