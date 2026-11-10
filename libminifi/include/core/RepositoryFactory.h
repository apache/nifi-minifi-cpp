// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>

#include "minifi-cpp/core/ContentRepository.h"
#include "minifi-cpp/core/Repository.h"
#include "core/Core.h"

namespace org::apache::nifi::minifi::core {

std::unique_ptr<core::ContentRepository> createContentRepository(const std::string& configuration_class_name,
    const std::string& repo_name,
    logging::Logger& logger);

std::unique_ptr<core::Repository> createRepository(const std::string& configuration_class_name, const std::string& repo_name = "");

}  // namespace org::apache::nifi::minifi::core
