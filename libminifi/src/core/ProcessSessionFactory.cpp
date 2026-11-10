// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/ProcessSessionFactory.h"
#include <memory>

namespace org::apache::nifi::minifi::core {

std::shared_ptr<ProcessSession> ProcessSessionFactoryImpl::createSession() {
  process_context_->setSessionStateManager(process_context_->createStateManager());
  return std::make_shared<ProcessSessionImpl>(process_context_);
}

}  // namespace org::apache::nifi::minifi::core
