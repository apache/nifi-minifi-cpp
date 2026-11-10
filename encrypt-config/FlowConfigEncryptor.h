// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <filesystem>

#include "Utils.h"
#include "core/flow/AdaptiveConfiguration.h"

namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor {

enum class EncryptionType {
  Interactive,
  SingleProperty,
  ReEncrypt
};

struct EncryptionRequest {
  explicit EncryptionRequest(EncryptionType type);
  EncryptionRequest(std::string_view component_id, std::string_view item_name, std::string_view item_value);

  EncryptionType type;
  std::string component_id;
  std::string item_name;
  std::string item_value;
};

void encryptSensitiveValuesInFlowConfig(const EncryptionKeys& keys, const std::filesystem::path& minifi_home, const std::filesystem::path& flow_config_path, const EncryptionRequest& request);

}  // namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor
