// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string_view>
#include <memory>

#include "minifi-cpp/core/logging/LoggerFactory.h"
#include "core/ClassName.h"

namespace org::apache::nifi::minifi::core::logging {

template<typename T>
class LoggerFactory : public LoggerFactoryBase {
 public:
  static std::shared_ptr<Logger> getLogger() {
    static std::shared_ptr<Logger> logger = getAliasedLogger(core::className<T>());
    return logger;
  }

  static std::shared_ptr<Logger> getLogger(const utils::Identifier& uuid) {
    return getAliasedLogger(core::className<T>(), uuid);
  }
};

}  // namespace org::apache::nifi::minifi::core::logging
