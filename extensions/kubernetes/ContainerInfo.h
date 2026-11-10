// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>

namespace org::apache::nifi::minifi::kubernetes {

struct ContainerInfo {
  std::string name_space;
  std::string pod_name;
  std::string container_name;
};

}  // namespace org::apache::nifi::minifi::kubernetes
