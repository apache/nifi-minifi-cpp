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

#include <optional>
#include <string>
#include <utility>

#include "utils/crypto/EncryptionUtils.h"
#include "utils/crypto/EncryptionProvider.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

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

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
