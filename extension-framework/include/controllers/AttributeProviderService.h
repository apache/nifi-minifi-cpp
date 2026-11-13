// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/controller/ControllerServiceBase.h"
#include "minifi-cpp/controllers/AttributeProviderService.h"

namespace org::apache::nifi::minifi::controllers {

class AttributeProviderServiceImpl : public core::controller::ControllerServiceBase, public AttributeProviderService {
 public:
  using ControllerServiceBase::ControllerServiceBase;

  [[nodiscard]] ControllerServiceHandle* getControllerServiceHandle() override {return this;}
};

}  // namespace org::apache::nifi::minifi::controllers

