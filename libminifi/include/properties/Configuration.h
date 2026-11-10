// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <vector>
#include <unordered_map>
#include <string>

#include "properties/Properties.h"
#include "minifi-cpp/properties/Configuration.h"
#include "utils/OptionalUtils.h"
#include "minifi-cpp/utils/Export.h"

namespace org::apache::nifi::minifi {

namespace core {
class PropertyValidator;
}

class ConfigurationImpl : public PropertiesImpl, public virtual Configuration {
 public:
  ConfigurationImpl() : PropertiesImpl(PersistTo::MultipleFiles, "MiNiFi configuration") {}
};

}  // namespace org::apache::nifi::minifi
