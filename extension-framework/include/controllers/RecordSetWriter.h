// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "minifi-cpp/controllers/RecordSetWriter.h"
#include "core/controller/ControllerServiceBase.h"

namespace org::apache::nifi::minifi::core {

class RecordSetWriterImpl : public controller::ControllerServiceBase, public RecordSetWriter {
 public:
  using ControllerServiceBase::ControllerServiceBase;

  [[nodiscard]] ControllerServiceHandle* getControllerServiceHandle() override {return this;}
};

}  // namespace org::apache::nifi::minifi::core
