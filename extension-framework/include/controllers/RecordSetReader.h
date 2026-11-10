// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "minifi-cpp/controllers/RecordSetReader.h"
#include "core/controller/ControllerServiceBase.h"


namespace org::apache::nifi::minifi::core {

class RecordSetReaderImpl : public controller::ControllerServiceBase, public RecordSetReader {
 public:
  using ControllerServiceBase::ControllerServiceBase;

  [[nodiscard]] ControllerServiceHandle* getControllerServiceHandle() override {return this;}
};

}  // namespace org::apache::nifi::minifi::core
