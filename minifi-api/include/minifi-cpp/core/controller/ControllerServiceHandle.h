// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

namespace org::apache::nifi::minifi::core::controller {

// Represents the usage side of the controller service.
// It should be dynamic_cast to whatever actual interface we expect from the
// service, e.g. RecordSetReader.
class ControllerServiceHandle {
 public:
  virtual ~ControllerServiceHandle() = default;
};

}  // namespace org::apache::nifi::minifi::core::controller
