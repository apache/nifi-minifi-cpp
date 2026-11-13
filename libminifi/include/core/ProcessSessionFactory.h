// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "minifi-cpp/core/ProcessContext.h"
#include "ProcessSession.h"
#include "minifi-cpp/core/ProcessSessionFactory.h"

namespace org::apache::nifi::minifi::core {

class ProcessSessionFactoryImpl : public virtual ProcessSessionFactory {
 public:
  explicit ProcessSessionFactoryImpl(std::shared_ptr<ProcessContext> processContext)
      : process_context_(processContext) {
  }

  std::shared_ptr<ProcessSession> createSession() override;

  ProcessSessionFactoryImpl(const ProcessSessionFactoryImpl &parent) = delete;
  ProcessSessionFactoryImpl &operator=(const ProcessSessionFactoryImpl &parent) = delete;

  ~ProcessSessionFactoryImpl() override = default;

 private:
  std::shared_ptr<ProcessContext> process_context_;
};

}  // namespace org::apache::nifi::minifi::core
