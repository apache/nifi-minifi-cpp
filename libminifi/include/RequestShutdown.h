// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <cstdint>
#include <functional>
#include <optional>

namespace org::apache::nifi::minifi {

enum class ShutdownType {
  RESTART,
  SHUTDOWN
};

using RequestShutdownCallback = std::function<void(ShutdownType shutdown_type, std::optional<int32_t> exit_code)>;

}  // namespace org::apache::nifi::minifi
