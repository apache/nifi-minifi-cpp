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

#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "fmt/chrono.h"
#include "fmt/ostream.h"
#include "fmt/std.h"
#include "minifi-c.h"
#include "utils/Enum.h"
#include "utils/GeneralUtils.h"
#include "utils/gsl.h"
#include "utils/SmallString.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::api::core::logging {

class Logger : public minifi::core::logging::Logger {
 public:
  explicit Logger(MinifiLogger impl): impl_(impl) {}

  void set_max_log_size(int size) override;
  std::optional<std::string> get_id() override;
  void log_string(minifi::core::logging::LOG_LEVEL level, std::string str) override;
  bool should_log(minifi::core::logging::LOG_LEVEL level) override;
  [[nodiscard]] minifi::core::logging::LOG_LEVEL level() const override;
  int getMaxLogSize() override;

 private:
  MinifiLogger impl_;
};

}  // namespace org::apache::nifi::minifi::core::logging
