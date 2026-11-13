// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "minifi-cpp/core/Relationship.h"

template<>
struct std::hash<org::apache::nifi::minifi::core::Relationship> {
  size_t operator()(const org::apache::nifi::minifi::core::Relationship& relationship) const noexcept {
    return std::hash<std::string>{}(relationship.getName());
  }
};
