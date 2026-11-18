// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <span>
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/Property.h"

namespace org::apache::nifi::minifi::core::reporting {

class ReportingTaskDescriptor {
 public:
  virtual ~ReportingTaskDescriptor() = default;

  virtual void setSupportedProperties(std::span<const PropertyReference> properties) = 0;
  virtual void setSupportedProperties(std::span<const Property> properties) = 0;
};

}  // namespace org::apache::nifi::minifi::core::reporting
