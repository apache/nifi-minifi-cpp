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
#pragma once

#include <string>
#include <filesystem>

#include "Utils.h"

namespace org::apache::nifi::minifi::encrypt_config {

class EncryptConfig {
 public:
  explicit EncryptConfig(const std::string& minifi_home);

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
};

}  // namespace org::apache::nifi::minifi::encrypt_config
