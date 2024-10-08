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

#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils::crypto::property_encryption {

namespace {
inline constexpr std::string_view WrapperBegin = "enc{";
inline constexpr std::string_view WrapperEnd = "}";

bool isEncrypted(std::string_view value) {
  return (value.starts_with(WrapperBegin) && value.ends_with(WrapperEnd));
}
}  // namespace

std::string decrypt(std::string_view input, const utils::crypto::EncryptionProvider& encryption_provider) {
  if (!isEncrypted(input)) {
    // this is normal: sensitive properties come from the C2 server in cleartext over TLS
    return std::string{input};
  }
  auto unwrapped_input = input.substr(WrapperBegin.size(), input.length() - (WrapperBegin.size() + WrapperEnd.size()));
  return encryption_provider.decrypt(unwrapped_input);
}

std::string encrypt(std::string_view input, const utils::crypto::EncryptionProvider& encryption_provider) {
  if (isEncrypted(input)) {
    return std::string{input};
  }
  return utils::string::join_pack("enc{", encryption_provider.encrypt(input), "}");
}

}  // namespace org::apache::nifi::minifi::utils::crypto::property_encryption
