// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <thread>

#include "Repository.h"
#include "TraceableResource.h"

namespace org::apache::nifi::minifi::core {

class ThreadedRepository : public virtual core::Repository, public virtual core::TraceableResource {
 public:
};

}  // namespace org::apache::nifi::minifi::core
