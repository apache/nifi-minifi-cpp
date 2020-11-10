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

#include <utility>
#include <string>
#include "utils/EncryptionUtils.h"
#include "utils/OptionalUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace crypto {

class EncryptionProvider {
 public:
  explicit EncryptionProvider(Bytes encryption_key)
      : encryption_key_(std::move(encryption_key)) {}

  static utils::optional<EncryptionProvider> create(const std::string& home_path);

  std::string encrypt(const std::string& data) const {
    return utils::crypto::encrypt(data, encryption_key_);
  }

  std::string decrypt(const std::string& data) const {
    return utils::crypto::decrypt(data, encryption_key_);
  }

 private:
  const Bytes encryption_key_;
};

}  // namespace crypto
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
