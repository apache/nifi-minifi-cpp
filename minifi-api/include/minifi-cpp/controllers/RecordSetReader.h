// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <expected>

#include "minifi-cpp/core/ControllerServiceTypeDefinition.h"
#include "minifi-cpp/core/Record.h"
#include "minifi-cpp/core/controller/ControllerServiceHandle.h"
#include "minifi-cpp/io/InputStream.h"

namespace org::apache::nifi::minifi::core {

class RecordSetReader : public controller::ControllerServiceHandle {
 public:
  static constexpr auto ProvidesApi = core::ControllerServiceTypeDefinition{
    .artifact = "minifi-system",
    .group = "org.apache.nifi.minifi",
    .type = "org.apache.nifi.minifi.core.RecordSetReader",
    .version = MINIFI_VERSION_STR
  };

  virtual std::expected<RecordSet, std::error_code> read(io::InputStream& input_stream) = 0;
};

}  // namespace org::apache::nifi::minifi::core
