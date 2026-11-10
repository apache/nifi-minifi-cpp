// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "minifi-cpp/core/logging/Logger.h"
#include "Extension.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::core::extension {

class ExtensionManager {
 public:
  explicit ExtensionManager(const std::shared_ptr<Configure>& config);
  ~ExtensionManager() = default;

  ExtensionManager(const ExtensionManager&) = delete;
  ExtensionManager(ExtensionManager&&) = delete;
  ExtensionManager& operator=(const ExtensionManager&) = delete;
  ExtensionManager& operator=(ExtensionManager&&) = delete;

 private:
  std::vector<std::unique_ptr<Extension>> extensions_;

  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::extension
