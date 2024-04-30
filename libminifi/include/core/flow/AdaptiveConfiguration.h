/**
 *
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
#include <memory>
#include <vector>

#include "StructuredConfiguration.h"

namespace org::apache::nifi::minifi::core::flow {

class AdaptiveConfiguration : public StructuredConfiguration {
 public:
  explicit AdaptiveConfiguration(ConfigurationContext ctx);

  std::vector<std::string> getSupportedFormats() const override {
    return {"application/json", "text/yml", "application/vnd.minifi-c2+yaml;version=2"};
  }

  std::unique_ptr<core::ProcessGroup> getRootFromPayload(const std::string &payload) override;

  void setSensitivePropertiesEncryptor(utils::crypto::EncryptionProvider sensitive_properties_encryptor);
  std::string serializeWithOverrides(const core::ProcessGroup& process_group, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
};

}  // namespace org::apache::nifi::minifi::core::flow
