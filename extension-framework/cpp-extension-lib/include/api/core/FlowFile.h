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

#include <exception>
#include <memory>
#include <stdexcept>

#include "minifi-api.h"

namespace org::apache::nifi::minifi::api::core {

struct EnsureMovedFromDeleter {
  void operator()(minifi_flow_file* ff) {
    if (ff) {
      if (std::uncaught_exceptions()) {
        // there is already an exception in progress, do not terminate the process (although there are scenarios we could throw here)
      } else {
        throw std::logic_error("Each flowfile should be either transferred or removed");
      }
    }
  }
};

using FlowFile = std::unique_ptr<minifi_flow_file, EnsureMovedFromDeleter>;

}  // namespace org::apache::nifi::minifi::api::core
