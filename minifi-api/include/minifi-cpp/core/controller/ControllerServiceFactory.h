// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/core/ControllerServiceMetadata.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceApi;

class ControllerServiceFactory {
 public:
  virtual std::unique_ptr<ControllerServiceApi> create(ControllerServiceMetadata metadata) = 0;
  virtual std::string getModuleName() const = 0;
  virtual std::string getClassName() const = 0;

  virtual ~ControllerServiceFactory() = default;
};

}  // namespace org::apache::nifi::minifi::core::controller
