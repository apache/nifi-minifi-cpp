// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string_view>
#include <utility>

#include "core/Core.h"
#include "minifi-cpp/utils/gsl.h"
#include "minifi-cpp/core/SerializableComponent.h"

namespace org::apache::nifi::minifi::core {

class SerializableComponentImpl : public core::CoreComponentImpl, public virtual SerializableComponent {
 public:
  explicit SerializableComponentImpl(std::string_view name)
    : core::CoreComponentImpl(name) {
  }
};

}  // namespace org::apache::nifi::minifi::core
