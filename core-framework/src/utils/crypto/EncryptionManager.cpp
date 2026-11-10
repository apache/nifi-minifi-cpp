// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <optional>
#include <string>

#include "utils/crypto/EncryptionManager.h"
#include "minifi-cpp/properties/Properties.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils::crypto {

std::optional<Bytes> EncryptionManager::readKey(const std::string& key_name) const {
  auto bootstrap_conf = minifi::Properties::create();
  bootstrap_conf->loadConfigureFile(key_dir_ / DEFAULT_NIFI_BOOTSTRAP_FILE);
  return bootstrap_conf->getString(key_name)
         | utils::transform([](const std::string &encryption_key_hex) { return utils::string::from_hex(encryption_key_hex); });
}

bool EncryptionManager::writeKey(const std::string &key_name, const Bytes& key) const {
  auto bootstrap_conf = minifi::Properties::create();
  bootstrap_conf->loadConfigureFile(key_dir_ / DEFAULT_NIFI_BOOTSTRAP_FILE);
  bootstrap_conf->set(key_name, utils::string::to_hex(key));
  return bootstrap_conf->commitChanges();
}

}  // namespace org::apache::nifi::minifi::utils::crypto
