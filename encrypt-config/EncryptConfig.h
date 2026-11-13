// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <filesystem>

#include "Utils.h"

namespace org::apache::nifi::minifi::encrypt_config {

class EncryptConfig {
 public:
  explicit EncryptConfig(std::filesystem::path minifi_home);
  ~EncryptConfig();

  void encryptSensitiveValuesInMinifiProperties() const;
  void encryptSensitiveValuesInFlowConfig(
      bool re_encrypt, const std::optional<std::string>& component_id, const std::optional<std::string>& item_name, const std::optional<std::string>& item_value) const;
  void encryptWholeFlowConfigFile() const;

  [[nodiscard]] bool isReEncrypting() const;

 private:
  [[nodiscard]] std::filesystem::path bootstrapFilePath() const;
  [[nodiscard]] std::filesystem::path propertiesFilePath() const;
  [[nodiscard]] std::filesystem::path flowConfigPath() const;

  [[nodiscard]] EncryptionKeys getEncryptionKeys(std::string_view property_name) const;
  [[nodiscard]] std::string hexDecodeAndValidateKey(const std::string& key, const std::string& key_name) const;
  void writeEncryptionKeyToBootstrapFile(const std::string& encryption_key_name, const utils::crypto::Bytes& encryption_key) const;

  const std::filesystem::path minifi_home_;
  const std::filesystem::path prev_current_path_;
};

}  // namespace org::apache::nifi::minifi::encrypt_config
