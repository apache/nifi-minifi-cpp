// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <optional>
#include <string>
#include <utility>

#include "utils/crypto/EncryptionUtils.h"
#include "utils/crypto/EncryptionProvider.h"

namespace org::apache::nifi::minifi {

class Decryptor {
 public:
  explicit Decryptor(utils::crypto::EncryptionProvider provider)
      : provider_(std::move(provider)) {}

  static bool isValidEncryptionMarker(const std::optional<std::string>& encryption_marker) {
    return encryption_marker && *encryption_marker == utils::crypto::EncryptionType::name();
  }

  [[nodiscard]] std::string decrypt(const std::string& encrypted_text) const {
    return provider_.decrypt(encrypted_text);
  }

  static std::optional<Decryptor> create(const std::filesystem::path& minifi_home) {
    return utils::crypto::EncryptionProvider::create(minifi_home)
        | utils::transform([](const utils::crypto::EncryptionProvider& provider) {return Decryptor{provider};});
  }

 private:
  const utils::crypto::EncryptionProvider provider_;
};

}  // namespace org::apache::nifi::minifi
