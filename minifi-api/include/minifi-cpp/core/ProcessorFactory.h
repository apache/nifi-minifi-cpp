// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>
#include <utility>
#include "minifi-cpp/core/ProcessorMetadata.h"

namespace org::apache::nifi::minifi::core {

class ProcessorApi;

class ProcessorFactory {
 public:
  virtual std::unique_ptr<ProcessorApi> create(ProcessorMetadata metadata) = 0;
  virtual std::string getGroupName() const = 0;
  virtual std::string getClassName() const = 0;

  virtual ~ProcessorFactory() = default;
};

}  // namespace org::apache::nifi::minifi::core
