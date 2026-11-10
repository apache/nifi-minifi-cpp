// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#ifdef WIN32

#include <memory>

using HANDLE = void*;

#include "core/Core.h"

struct GetTerminationEventHandleReturnType {
  bool is_started_by_service;
  HANDLE termination_event_handler;
};

void RunAsServiceIfNeeded();
GetTerminationEventHandleReturnType GetTerminationEventHandle();
bool CreateServiceTerminationThread(std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger> logger, HANDLE terminationEventHandle);

#endif
