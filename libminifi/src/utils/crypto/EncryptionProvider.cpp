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

#include "utils/crypto/EncryptionProvider.h"
#include "utils/OptionalUtils.h"
#include "utils/crypto/EncryptionManager.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::utils::crypto {

constexpr const char* CONFIG_ENCRYPTION_KEY_PROPERTY_NAME = "nifi.bootstrap.sensitive.key";

std::optional<EncryptionProvider> EncryptionProvider::create(const std::filesystem::path& home_path) {
  return EncryptionManager{home_path}.createXSalsa20Cipher(CONFIG_ENCRYPTION_KEY_PROPERTY_NAME)
    | utils::transform([] (const XSalsa20Cipher& cipher) {return EncryptionProvider{cipher};});
}

}  // namespace org::apache::nifi::minifi::utils::crypto
