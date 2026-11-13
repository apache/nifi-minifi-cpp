// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <exception>
#include <memory>
#include <stdexcept>

#include "minifi-api.h"

namespace org::apache::nifi::minifi::api::core {

struct EnsureMovedFromDeleter {
  void operator()(minifi_flow_file* ff) {
    if (ff) {
      if (std::uncaught_exceptions()) {
        // there is already an exception in progress, do not terminate the process (although there are scenarios we could throw here)
      } else {
        throw std::logic_error("Each flowfile should be either transferred or removed");
      }
    }
  }
};

using FlowFile = std::unique_ptr<minifi_flow_file, EnsureMovedFromDeleter>;

}  // namespace org::apache::nifi::minifi::api::core
