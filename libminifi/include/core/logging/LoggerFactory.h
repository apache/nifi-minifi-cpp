/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string_view>
#include <memory>

#include "minifi-cpp/core/logging/LoggerFactory.h"
#include "core/logging/Logger.h"
#include "core/Core.h"

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
