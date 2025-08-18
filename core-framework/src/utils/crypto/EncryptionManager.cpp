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

#include <optional>
#include <string>

#include "utils/crypto/EncryptionManager.h"
#include "minifi-cpp/properties/Properties.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils::crypto {

std::optional<Bytes> EncryptionManager::readKey(const std::string& key_name) const {
  auto bootstrap_conf = minifi::Properties::create();
  bootstrap_conf->setHome(key_dir_);
  bootstrap_conf->loadConfigureFile(DEFAULT_NIFI_BOOTSTRAP_FILE);
  return bootstrap_conf->getString(key_name)
         | utils::transform([](const std::string &encryption_key_hex) { return utils::string::from_hex(encryption_key_hex); });
}

bool EncryptionManager::writeKey(const std::string &key_name, const Bytes& key) const {
  auto bootstrap_conf = minifi::Properties::create();
  bootstrap_conf->setHome(key_dir_);
  bootstrap_conf->loadConfigureFile(DEFAULT_NIFI_BOOTSTRAP_FILE);
  bootstrap_conf->set(key_name, utils::string::to_hex(key));
  return bootstrap_conf->commitChanges();
}

}  // namespace org::apache::nifi::minifi::utils::crypto
