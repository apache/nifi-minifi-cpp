// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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

  void setSensitivePropertiesEncryptor(utils::crypto::EncryptionProvider sensitive_values_encryptor);
  std::string serializeWithOverrides(const core::ProcessGroup& process_group, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
};

}  // namespace org::apache::nifi::minifi::core::flow
