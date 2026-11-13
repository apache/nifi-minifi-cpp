// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>

#include "ProcessSession.h"

namespace org::apache::nifi::minifi::core {

class ProcessSessionFactory {
 public:
  virtual std::shared_ptr<ProcessSession> createSession() = 0;
  virtual ~ProcessSessionFactory() = default;
};

}  // namespace org::apache::nifi::minifi::core
