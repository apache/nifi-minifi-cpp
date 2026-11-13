// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "PropertiesFileEncryptor.h"

#include <iostream>
#include <optional>
#include <string>

#include "properties/Configuration.h"
#include "properties/Properties.h"
#include "utils/StringUtils.h"

namespace {
bool isEncrypted(const std::optional<std::string>& encryption_type) {
  return encryption_type && !encryption_type->empty() && *encryption_type  != "plaintext";
}
}  // namespace

namespace org::apache::nifi::minifi::encrypt_config {

std::vector<std::string> getSensitiveProperties(const std::filesystem::path& properties_file_path) {
  auto minifi_properties = PropertiesImpl{PropertiesImpl::PersistTo::MultipleFiles, "MiNiFi properties"};
  minifi_properties.loadConfigureFile(properties_file_path);

  auto sensitive_properties = Configuration::getSensitiveProperties([&minifi_properties](const std::string& property_name) {
    return minifi_properties.getString(property_name);
  });
  auto not_found = [&minifi_properties](const std::string& property_name) { return !minifi_properties.getString(property_name).has_value(); };
  std::erase_if(sensitive_properties, not_found);

  return sensitive_properties;
}

uint32_t encryptSensitivePropertiesInFile(PropertiesFile& properties_file, const std::vector<std::string>& sensitive_properties, const utils::crypto::Bytes & encryption_key) {
  return encryptSensitivePropertiesInFile(properties_file, sensitive_properties, EncryptionKeys{{}, encryption_key});
}

uint32_t encryptSensitivePropertiesInFile(PropertiesFile& properties_file, const std::vector<std::string>& sensitive_properties, const EncryptionKeys& keys) {
  uint32_t num_properties_encrypted = 0;

  for (const auto& property_key : sensitive_properties) {
    std::optional<std::string> property_value = properties_file.getValue(property_key);
    if (!property_value) { continue; }

    std::string encryption_type_key = property_key + ".protected";
    std::optional<std::string> encryption_type = properties_file.getValue(encryption_type_key);

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

    properties_file.update(property_key, encrypted_property_value);

    if (encryption_type) {
      properties_file.update(encryption_type_key, utils::crypto::EncryptionType::name());
    } else {
      properties_file.insertAfter(property_key, encryption_type_key, utils::crypto::EncryptionType::name());
    }

    std::cout << "Encrypted property: " << property_key << '\n';
    ++num_properties_encrypted;
  }

  return num_properties_encrypted;
}

}  // namespace org::apache::nifi::minifi::encrypt_config
