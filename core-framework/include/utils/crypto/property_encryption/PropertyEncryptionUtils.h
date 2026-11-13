// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <string_view>

#include "utils/crypto/EncryptionProvider.h"

namespace org::apache::nifi::minifi::utils::crypto::property_encryption {

std::string decrypt(std::string_view input, const EncryptionProvider& encryption_provider);
std::string encrypt(std::string_view input, const EncryptionProvider& encryption_provider);

}  // namespace org::apache::nifi::minifi::utils::crypto::property_encryption
