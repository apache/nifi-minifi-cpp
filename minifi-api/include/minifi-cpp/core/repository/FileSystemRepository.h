// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "minifi-cpp/core/ContentRepository.h"

namespace org::apache::nifi::minifi::core::repository {

std::shared_ptr<core::ContentRepository> createFileSystemRepository();

}  // namespace org::apache::nifi::minifi::core::repository
