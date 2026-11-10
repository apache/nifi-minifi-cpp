// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <system_error>

namespace org::apache::nifi::minifi::utils {

std::error_code getLastError();

[[nodiscard]] bool compareErrors(std::error_code lhs, std::error_code rhs);

}  // namespace org::apache::nifi::minifi::utils
