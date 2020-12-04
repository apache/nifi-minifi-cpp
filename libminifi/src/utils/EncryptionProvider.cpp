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

#include <string>
#include "utils/EncryptionProvider.h"
#include "properties/Properties.h"
#include "utils/OptionalUtils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace crypto {

namespace {

#ifdef WIN32
constexpr const char* DEFAULT_NIFI_BOOTSTRAP_FILE = "\\conf\\bootstrap.conf";
#else
constexpr const char* DEFAULT_NIFI_BOOTSTRAP_FILE = "./conf/bootstrap.conf";
#endif  // WIN32

constexpr const char* CONFIG_ENCRYPTION_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.key";

}  // namespace

utils::optional<EncryptionProvider> EncryptionProvider::create(const std::string& home_path) {
  minifi::Properties bootstrap_conf;
  bootstrap_conf.setHome(home_path);
  bootstrap_conf.loadConfigureFile(DEFAULT_NIFI_BOOTSTRAP_FILE);
  return bootstrap_conf.getString(CONFIG_ENCRYPTION_KEY_PROPERTY_NAME)
    | utils::map([](const std::string &encryption_key_hex) { return utils::StringUtils::from_hex(encryption_key_hex); })
    | utils::map(&utils::crypto::stringToBytes)
    | utils::map([](const utils::crypto::Bytes &encryption_key_bytes) { return EncryptionProvider{encryption_key_bytes}; });
}

}  // namespace crypto
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
