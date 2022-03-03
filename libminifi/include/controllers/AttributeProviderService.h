/**
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
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/controller/ControllerService.h"

namespace org::apache::nifi::minifi::controllers {

class AttributeProviderService : public core::controller::ControllerService {
 public:
  using ControllerService::ControllerService;

  void yield() override {}
  bool isRunning() override { return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }

  using AttributeMap = std::unordered_map<std::string, std::string>;
  virtual std::optional<std::vector<AttributeMap>> getAttributes() = 0;
  virtual std::string_view name() const = 0;
};

}  // namespace org::apache::nifi::minifi::controllers
