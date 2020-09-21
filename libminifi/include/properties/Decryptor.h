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

#include "utils/EncryptionUtils.h"
#include "utils/OptionalUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class Configure;

class Decryptor {
 public:
  explicit Decryptor(const utils::crypto::Bytes& encryption_key);
  static bool isEncrypted(const utils::optional<std::string>& encryption_type);
  std::string decrypt(const std::string& encrypted_text, const std::string& aad) const;
  void decryptSensitiveProperties(Configure& configure) const;

 private:
  const utils::crypto::Bytes encryption_key_;
};

inline Decryptor::Decryptor(const utils::crypto::Bytes& encryption_key) : encryption_key_(encryption_key) {}

inline bool Decryptor::isEncrypted(const utils::optional<std::string>& encryption_type) {
  return encryption_type && *encryption_type == utils::crypto::EncryptionType::name();
}

inline std::string Decryptor::decrypt(const std::string& encrypted_text, const std::string& aad) const {
  return utils::crypto::decrypt(encrypted_text, aad, encryption_key_);
}

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
