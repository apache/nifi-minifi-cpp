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
#include <utility>

#include "properties/Configuration.h"
#include "properties/Decryptor.h"
#include "utils/OptionalUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class Configure : public Configuration {
 public:
  explicit Configure(utils::optional<Decryptor> decryptor = utils::nullopt)
      : Configuration{}, decryptor_(std::move(decryptor)) {}

  bool get(const std::string& key, std::string& value) const;
  bool get(const std::string& key, const std::string& alternate_key, std::string& value) const;
  utils::optional<std::string> get(const std::string& key) const;

 private:
  bool isEncrypted(const std::string& key) const;

  utils::optional<Decryptor> decryptor_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
