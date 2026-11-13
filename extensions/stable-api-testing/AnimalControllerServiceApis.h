// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <cinttypes>
#include <string_view>

namespace org::apache::nifi::minifi::api_testing {
class NumberOfLegsControllerApi {
 public:
  virtual ~NumberOfLegsControllerApi() = default;
  virtual uint8_t numberOfLegs() const = 0;
};

class CanFlyControllerApi {
 public:
  virtual ~CanFlyControllerApi() = default;
  virtual bool canFly() const = 0;
};

}  // namespace org::apache::nifi::minifi::api_testing
