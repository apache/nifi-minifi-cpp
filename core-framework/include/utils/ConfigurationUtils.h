// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <cstddef>

namespace org::apache::nifi::minifi {

class Configure;

namespace utils::configuration {

inline constexpr size_t DEFAULT_BUFFER_SIZE = 4096;
size_t getBufferSize(const Configure& configuration);

}  // namespace utils::configuration
}  // namespace org::apache::nifi::minifi
