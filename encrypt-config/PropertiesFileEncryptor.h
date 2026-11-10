// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "properties/PropertiesFile.h"
#include "utils/crypto/EncryptionUtils.h"
#include "Utils.h"

namespace org::apache::nifi::minifi::encrypt_config {

std::vector<std::string> getSensitiveProperties(const std::filesystem::path& properties_file_path);

uint32_t encryptSensitivePropertiesInFile(PropertiesFile& properties_file, const std::vector<std::string>& sensitive_properties, const utils::crypto::Bytes& encryption_key);

uint32_t encryptSensitivePropertiesInFile(PropertiesFile& properties_file, const std::vector<std::string>& sensitive_properties, const EncryptionKeys& keys);

}  // namespace org::apache::nifi::minifi::encrypt_config
