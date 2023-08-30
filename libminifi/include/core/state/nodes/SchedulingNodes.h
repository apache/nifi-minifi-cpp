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

#include <string>
#include <utility>
#include <vector>

#include "MetricsBase.h"
#include "core/ProcessorConfig.h"

namespace org::apache::nifi::minifi::state::response {

class SchedulingDefaults : public DeviceInformation {
 public:
  SchedulingDefaults(std::string_view name, const utils::Identifier &uuid)
      : DeviceInformation(name, uuid) {
  }

  explicit SchedulingDefaults(std::string_view name)
      : DeviceInformation(name) {
  }

  std::string getName() const override {
    return "schedulingDefaults";
  }

  std::vector<SerializedResponseNode> serialize() override;
};

}  // namespace org::apache::nifi::minifi::state::response
