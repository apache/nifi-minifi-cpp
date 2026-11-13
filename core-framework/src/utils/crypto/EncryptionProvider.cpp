// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/crypto/EncryptionProvider.h"
#include "utils/OptionalUtils.h"
#include "utils/crypto/ciphers/XSalsa20.h"
#include "utils/crypto/EncryptionManager.h"

namespace org::apache::nifi::minifi::utils::crypto {

inline constexpr std::string_view CONFIG_ENCRYPTION_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.key";
inline constexpr std::string_view CONFIG_SENSITIVE_PROPERTIES_ENCRYPTION_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.properties.key";

std::optional<EncryptionProvider> EncryptionProvider::create(const std::filesystem::path& home_path) {
  return EncryptionManager{home_path}.getOptionalKey<XSalsa20Cipher>(std::string{CONFIG_ENCRYPTION_KEY_PROPERTY_NAME})
      | utils::transform([] (const XSalsa20Cipher& cipher) { return EncryptionProvider{cipher}; });
}

EncryptionProvider EncryptionProvider::createSensitivePropertiesEncryptor(const std::filesystem::path &home_path) {
  const auto cipher = EncryptionManager{home_path}.getRequiredKey<XSalsa20Cipher>(std::string{CONFIG_SENSITIVE_PROPERTIES_ENCRYPTION_KEY_PROPERTY_NAME});
  return EncryptionProvider{cipher};
}

}  // namespace org::apache::nifi::minifi::utils::crypto
