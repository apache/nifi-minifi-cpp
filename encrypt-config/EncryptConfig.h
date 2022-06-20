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

#include "Utils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

class EncryptConfig {
 public:
  enum class EncryptionType {
    ENCRYPT,
    RE_ENCRYPT
  };

  explicit EncryptConfig(const std::string& minifi_home);
  EncryptionType encryptSensitiveProperties() const;

  void encryptFlowConfig() const;

 private:
  std::string bootstrapFilePath() const;
  std::string propertiesFilePath() const;

  EncryptionKeys getEncryptionKeys() const;
  std::string hexDecodeAndValidateKey(const std::string& key, const std::string& key_name) const;
  void writeEncryptionKeyToBootstrapFile(const utils::crypto::Bytes& encryption_key) const;

  void encryptSensitiveProperties(const EncryptionKeys& keys) const;

  const std::string minifi_home_;
  EncryptionKeys keys_;
};

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
